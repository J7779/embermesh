#include "console.h"

#include <ctime>
#include <iostream>
#include <sstream>

#include "host/endpoint.h"
#include "host/util.h"

using ember_host::Json;

namespace emberd {

namespace {

std::string first_word(const std::string& s, std::string* rest) {
  std::string t = ember_host::trim(s);
  size_t sp = t.find_first_of(" \t");
  if (sp == std::string::npos) {
    if (rest) rest->clear();
    return t;
  }
  if (rest) *rest = ember_host::trim(t.substr(sp + 1));
  return t.substr(0, sp);
}

std::string short_id(const std::string& id) { return id.size() > 8 ? id.substr(0, 8) : id; }

std::string one_line(std::string s, size_t max = 60) {
  for (auto& c : s)
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  if (s.size() > max) s = s.substr(0, max - 3) + "...";
  return s;
}

}  // namespace

std::string format_time(double ts_ms) {
  if (ts_ms <= 0) return "--:--";
  std::time_t t = std::time_t(ts_ms / 1000);
  std::tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char buf[32];
  std::time_t now = std::time(nullptr);
  if (now - t < 20 * 3600 && now >= t)
    std::strftime(buf, sizeof buf, "%H:%M", &tmv);
  else
    std::strftime(buf, sizeof buf, "%m-%d %H:%M", &tmv);
  return buf;
}

Console::Console(CallFn call, std::ostream& out) : call_(std::move(call)), out_(out) {}

void Console::print(const std::string& s) {
  std::lock_guard<std::mutex> lk(out_mu_);
  out_ << s << "\n";
  out_.flush();
}

Json Console::call(const std::string& method, const Json& params) { return call_(method, params); }

bool Console::ok(const Json& env) {
  if (env.get("ok").as_bool(false)) return true;
  const Json& e = env.get("error");
  std::string code = e.get("code").as_string("internal");
  print("error: " + code + ": " + e.get("message").as_string("") + (code == "locked" ? "  (use: unlock <passphrase>)" : ""));
  return false;
}

void Console::on_event(const std::string& event_json) {
  Json ev = Json::parse_or_null(event_json);
  const std::string& name = ev.get("event").as_string();
  const Json& d = ev.get("data");
  std::string line;
  if (name == "message.new") {
    const Json& m = d.get("message");
    if (m.get("outgoing").as_bool(false)) return;
    std::string who = m.get("from_name").as_string(short_id(m.get("from").as_string("?")));
    std::string kind = m.get("kind").as_string("text");
    line = "<< [" + m.get("conv").as_string("?") + "] " + format_time(m.get("ts").as_number()) + " " + who + ": " +
           (kind == "sos" ? "SOS! " : "") + m.get("text").as_string();
  } else if (name == "sos.received") {
    line = "!!! SOS from " + d.get("name").as_string(short_id(d.get("node_id").as_string())) + " (" +
           d.get("node_id").as_string() + "): " + d.get("text").as_string() + "  [" +
           std::to_string(d.get("hops").as_int(0)) + " hops]  -> 'sos ack " + d.get("node_id").as_string() + "'";
  } else if (name == "checkin.received") {
    line = "-- check-in " + d.get("name").as_string(short_id(d.get("node_id").as_string())) + ": " +
           d.get("status").as_string() + (d.get("note").as_string().empty() ? "" : " - " + d.get("note").as_string());
  } else if (name == "bulletin.new") {
    const Json& b = d.get("bulletin");
    line = "-- bulletin [" + b.get("category").as_string() + "] " + b.get("title").as_string() + " (from " +
           b.get("from_name").as_string("?") + ")";
  } else if (name == "link.state") {
    const Json& l = d.get("link");
    line = "-- link " + std::to_string(l.get("id").as_int(0)) + " " + l.get("name").as_string() + ": " +
           l.get("state").as_string() + (l.get("error").as_string().empty() ? "" : " (" + l.get("error").as_string() + ")");
  } else if (name == "host.endpoint.state") {
    line = "-- endpoint of link " + std::to_string(d.get("link_id").as_int(0)) + (d.get("connected").as_bool(false) ? " up" : " down") +
           (d.get("error").as_string().empty() ? "" : ": " + d.get("error").as_string());
  } else if (name == "host.hub.clients") {
    line = "-- hub: " + std::to_string(d.get("clients").as_int(0)) + " device(s) connected";
  } else if (name == "mesh.pong") {
    line = "-- pong from " + short_id(d.get("node_id").as_string()) + " " + std::to_string(d.get("rtt_ms").as_int(0)) + " ms";
  } else if (name == "morse.text") {
    // Decoded characters arrive one or a few at a time: print them a word at a time.
    std::lock_guard<std::mutex> lk(morse_mu_);
    std::string sess = d.get("session").as_string(ember_host::cfg_str(d, "session"));
    if (morse_session_.empty() || sess != morse_session_) {
      if (!all_events_) return;
      line = "-- morse " + sess + ": " + d.get("text").as_string();
    } else {
      if (d.get("wpm").as_number() > 0) morse_wpm_ = int(d.get("wpm").as_number() + 0.5);
      morse_line_ += d.get("text").as_string();
      morse_flush_locked(false);
      return;
    }
  } else if (name == "host.capture.state") {
    if (d.get("running").as_bool(false)) return;
    {
      std::lock_guard<std::mutex> lk(morse_mu_);
      morse_flush_locked(true);
    }
    line = "-- capture stopped" + (d.get("error").as_string().empty() ? std::string() : ": " + d.get("error").as_string()) +
           " ('morse stop' prints the transcript)";
  } else if (name == "message.status") {
    if (!all_events_) return;
    line = "-- message " + short_id(d.get("id").as_string()) + ": " + d.get("status").as_string();
  } else {
    if (!all_events_) return;
    line = "-- " + event_json;
  }
  print(line);
}

std::string Console::name_of(const std::string& node_id) {
  Json env = call("contacts.list", Json::object());
  for (const auto& c : env.get("result").get("contacts").elements())
    if (c.get("node_id").as_string() == node_id) return c.get("name").as_string(node_id);
  return node_id;
}

std::string Console::public_channel_conv() {
  Json env = call("channels.list", Json::object());
  for (const auto& c : env.get("result").get("channels").elements())
    if (c.get("is_default").as_bool(false)) return "ch:" + c.get("id").as_string();
  for (const auto& c : env.get("result").get("channels").elements())
    if (ember_host::iequals(c.get("name").as_string(), "public")) return "ch:" + c.get("id").as_string();
  return "ch:public";
}

std::string Console::resolve_conv(const std::string& arg) {
  if (arg.empty()) return arg;
  if (arg[0] == '#') {
    uint64_t n = 0;
    if (ember_host::parse_u64(arg.substr(1), &n) && n >= 1 && n <= last_convs_.size()) return last_convs_[size_t(n - 1)];
    return std::string();
  }
  if (ember_host::starts_with(arg, "dm:") || ember_host::starts_with(arg, "ch:")) return arg;
  if (arg == "public") return public_channel_conv();
  // A node id or a contact name.
  if (arg.size() == 16 && arg.find_first_not_of("0123456789abcdef") == std::string::npos) return "dm:" + arg;
  Json env = call("contacts.list", Json::object());
  for (const auto& c : env.get("result").get("contacts").elements())
    if (ember_host::iequals(c.get("name").as_string(), arg)) return "dm:" + c.get("node_id").as_string();
  Json ch = call("channels.list", Json::object());
  for (const auto& c : ch.get("result").get("channels").elements())
    if (ember_host::iequals(c.get("name").as_string(), arg)) return "ch:" + c.get("id").as_string();
  return std::string();
}

void Console::cmd_help() {
  print(
      "Commands:\n"
      "  info                         this node (id, name, fingerprint)\n"
      "  peers | contacts | channels  who is around / known / joined\n"
      "  convs                        conversations (use #n to refer to them)\n"
      "  read <conv|#n> [count]       show messages\n"
      "  send <conv|#n> <text>        send to a conversation (dm:<id>, ch:<id>, a contact or channel name)\n"
      "  say <text>                   send to the Public channel\n"
      "  dm <node_id|name> <text>     direct message\n"
      "  sos [text] | sos stop | sos status | sos list | sos ack <node_id>\n"
      "  checkin <ok|need_help|injured|moving|sheltering> [note]\n"
      "  links | link add <json> | link rm <id> | link reconnect <id>\n"
      "  hub | hub start [port] | hub stop\n"
      "  name <new name>              set this node's display name\n"
      "  unlock <passphrase> | lock   unlock / lock an encrypted store\n"
      "  morse listen [device | file:PATH] | morse stop | morse status\n"
      "                               read Morse (CW) from this computer's sound input, live\n"
      "  events on|off                show every event\n"
      "  call <method> [json]         raw RPC call\n"
      "  quit");
}

void Console::cmd_info() {
  Json env = call("node.info", Json::object());
  if (!ok(env)) return;
  const Json& r = env.get("result");
  std::string fp;
  for (const auto& w : r.get("fingerprint").elements()) fp += (fp.empty() ? "" : " ") + w.as_string();
  std::ostringstream s;
  s << "node " << r.get("node_id").as_string() << "  name: " << r.get("name").as_string()
    << (r.get("callsign").as_string().empty() ? "" : "  callsign: " + r.get("callsign").as_string()) << "\n"
    << "version " << r.get("version").as_string() << (r.get("stub").as_bool(false) ? " (STUB CORE)" : "")
    << (fp.empty() ? "" : "\nfingerprint: " + fp);
  print(s.str());
}

void Console::cmd_peers() {
  Json env = call("mesh.peers", Json::object());
  if (!ok(env)) return;
  const Json& peers = env.get("result").get("peers");
  if (!peers.size()) {
    print("(no peers heard yet)");
    return;
  }
  for (const auto& p : peers.elements()) {
    std::string hops = p.get("hops").is_number() ? std::to_string(p.get("hops").as_int(0)) : std::string("?");
    print(ember_host::format("  %-16s %-20s %s hops=%s seen %s", p.get("node_id").as_string().c_str(),
                             one_line(p.get("name").as_string(), 20).c_str(), p.get("direct").as_bool(false) ? "direct" : "      ",
                             hops.c_str(), format_time(p.get("last_seen_ts").as_number()).c_str()));
  }
}

void Console::cmd_contacts() {
  Json env = call("contacts.list", Json::object());
  if (!ok(env)) return;
  const Json& cs = env.get("result").get("contacts");
  if (!cs.size()) print("(no contacts)");
  for (const auto& c : cs.elements())
    print(ember_host::format("  %-16s %-20s%s%s", c.get("node_id").as_string().c_str(), one_line(c.get("name").as_string(), 20).c_str(),
                             c.get("verified").as_bool(false) ? " verified" : "", c.get("blocked").as_bool(false) ? " BLOCKED" : ""));
}

void Console::cmd_channels() {
  Json env = call("channels.list", Json::object());
  if (!ok(env)) return;
  for (const auto& c : env.get("result").get("channels").elements())
    print(ember_host::format("  ch:%-14s %-20s %s unread=%lld", c.get("id").as_string().c_str(), one_line(c.get("name").as_string(), 20).c_str(),
                             c.get("encrypted").as_bool(false) ? "private" : "open   ", static_cast<long long>(c.get("unread").as_int(0))));
}

void Console::cmd_convs() {
  Json env = call("conversations.list", Json::object());
  if (!ok(env)) return;
  last_convs_.clear();
  int i = 0;
  for (const auto& c : env.get("result").get("conversations").elements()) {
    ++i;
    last_convs_.push_back(c.get("id").as_string());
    const Json& lm = c.get("last_message");
    print(ember_host::format("  #%-3d %-22s %-20s %3lld unread  %s", i, c.get("id").as_string().c_str(),
                             one_line(c.get("title").as_string(), 20).c_str(), static_cast<long long>(c.get("unread").as_int(0)),
                             lm.is_object() ? one_line(lm.get("from_name").as_string() + ": " + lm.get("text").as_string(), 40).c_str() : ""));
  }
  if (!i) print("(no conversations yet — try: say hello)");
}

void Console::cmd_read(const std::string& conv, int n) {
  Json p = Json::object();
  p.set("conv", conv);
  p.set("limit", n);
  Json env = call("messages.list", p);
  if (!ok(env)) return;
  for (const auto& m : env.get("result").get("messages").elements()) {
    std::string who = m.get("outgoing").as_bool(false) ? std::string("me") : m.get("from_name").as_string(short_id(m.get("from").as_string()));
    print("  " + format_time(m.get("ts").as_number()) + " " + who + ": " + m.get("text").as_string() + "  (" +
          m.get("status").as_string() + ")");
  }
  Json mr = Json::object();
  mr.set("conv", conv);
  call("messages.mark_read", mr);
}

void Console::cmd_send(const std::string& conv, const std::string& text) {
  if (conv.empty()) {
    print("unknown conversation (use convs, dm:<node_id>, ch:<id>, a contact or channel name)");
    return;
  }
  if (text.empty()) {
    print("nothing to send");
    return;
  }
  Json p = Json::object();
  p.set("conv", conv);
  p.set("text", text);
  Json env = call("messages.send", p);
  if (!ok(env)) return;
  const Json& m = env.get("result").get("message");
  print(">> [" + conv + "] " + m.get("status").as_string("queued"));
}

void Console::cmd_sos(const std::string& args) {
  std::string rest;
  std::string sub = first_word(args, &rest);
  Json env;
  if (sub == "stop") {
    env = call("sos.stop", Json::object());
    if (ok(env)) print("SOS stopped");
    return;
  }
  if (sub == "status") {
    env = call("sos.status", Json::object());
    if (!ok(env)) return;
    const Json& r = env.get("result");
    std::string acks;
    for (const auto& a : r.get("acked_by").elements()) acks += " " + a.get("name").as_string(a.get("node_id").as_string());
    print(std::string("SOS ") + (r.get("active").as_bool(false) ? "ACTIVE" : "off") + "  sent=" +
          std::to_string(r.get("sent").as_int(0)) + (acks.empty() ? "" : "  heard by:" + acks));
    return;
  }
  if (sub == "list") {
    env = call("sos.list", Json::object());
    if (!ok(env)) return;
    const Json& a = env.get("result").get("alerts");
    if (!a.size()) print("(no SOS alerts)");
    for (const auto& x : a.elements())
      print("  " + x.get("node_id").as_string() + " " + x.get("name").as_string() + ": " + x.get("text").as_string() + "  last " +
            format_time(x.get("last_ts").as_number()) + (x.get("acked").as_bool(false) ? "  (acked)" : ""));
    return;
  }
  if (sub == "ack") {
    Json p = Json::object();
    p.set("node_id", ember_host::trim(rest));
    env = call("sos.ack", p);
    if (ok(env)) print("acknowledged");
    return;
  }
  Json p = Json::object();
  std::string text = ember_host::trim(args);
  if (!text.empty()) p.set("text", text);
  env = call("sos.start", p);
  if (ok(env)) print("SOS ACTIVE — repeating at top priority until 'sos stop'");
}

void Console::cmd_checkin(const std::string& args) {
  std::string note;
  std::string status = first_word(args, &note);
  static const char* valid[] = {"ok", "need_help", "injured", "moving", "sheltering"};
  bool good = false;
  for (const char* v : valid)
    if (status == v) good = true;
  if (!good) {
    print("usage: checkin <ok|need_help|injured|moving|sheltering> [note]");
    return;
  }
  Json p = Json::object();
  p.set("status", status);
  if (!note.empty()) p.set("note", note);
  if (ok(call("checkin.send", p))) print("check-in sent");
}

void Console::cmd_links() {
  Json env = call("links.list", Json::object());
  if (!ok(env)) return;
  const Json& ls = env.get("result").get("links");
  if (!ls.size()) print("(no links)");
  for (const auto& l : ls.elements()) {
    print(ember_host::format("  %-3lld %-10s %-28s %-10s %s %s", static_cast<long long>(l.get("id").as_int(0)), l.get("type").as_string().c_str(),
                             one_line(l.get("name").as_string(), 28).c_str(), l.get("state").as_string().c_str(),
                             l.get("endpoint").get("kind").as_string().c_str(), l.get("error").as_string().c_str()));
  }
  Json hub = call("host.hub.status", Json::object());
  if (hub.get("ok").as_bool(false) && hub.get("result").get("running").as_bool(false))
    print(ember_host::format("  hub: %lld guest(s)", static_cast<long long>(hub.get("result").get("clients").as_int(0))));
}

void Console::cmd_link(const std::string& args) {
  std::string rest;
  std::string sub = first_word(args, &rest);
  if (sub == "add") {
    Json spec;
    std::string err;
    if (!Json::parse(rest, &spec, &err) || !spec.is_object()) {
      print("usage: link add {\"endpoint\":{\"kind\":\"tcp\",\"config\":{\"host\":\"127.0.0.1\",\"port\":8001}},\"link\":{\"type\":\"kiss\"}}");
      return;
    }
    Json env = call("host.link.create", spec);
    if (ok(env)) print("link " + std::to_string(env.get("result").get("link").get("id").as_int(0)) + " created");
    return;
  }
  if (sub == "rm" || sub == "remove" || sub == "reconnect") {
    uint64_t id = 0;
    if (!ember_host::parse_u64(ember_host::trim(rest), &id)) {
      print("usage: link " + sub + " <id>");
      return;
    }
    Json p = Json::object();
    p.set("id", double(id));
    Json env = call(sub == "reconnect" ? "host.link.reconnect" : "host.link.remove", p);
    if (ok(env)) print(sub == "reconnect" ? "reconnecting" : "removed");
    return;
  }
  print("usage: link add <json> | link rm <id> | link reconnect <id>");
}

void Console::cmd_hub(const std::string& args) {
  std::string rest;
  std::string sub = first_word(args, &rest);
  Json env;
  if (sub == "start") {
    Json p = Json::object();
    uint64_t port;
    if (ember_host::parse_u64(rest, &port)) p.set("port", double(port));
    env = call("host.hub.start", p);
  } else if (sub == "stop") {
    env = call("host.hub.stop", Json::object());
  } else {
    env = call("host.hub.status", Json::object());
  }
  if (!ok(env)) return;
  const Json& r = env.get("result");
  std::string urls;
  for (const auto& u : r.get("urls").elements()) urls += "\n    " + u.as_string();
  print(std::string("hub ") + (r.get("running").as_bool(false) ? "running" : "stopped") + ", " +
        std::to_string(r.get("clients").as_int(0)) + " guest(s)" + urls);
}

// Caller holds morse_mu_. Prints complete words (or everything when forced / the line is long).
void Console::morse_flush_locked(bool force) {
  size_t cut = force ? morse_line_.size() : morse_line_.find_last_of(" \n");
  if (!force && morse_line_.size() > 60) cut = morse_line_.size();
  if (cut == std::string::npos || cut == 0) return;
  std::string out = ember_host::trim(morse_line_.substr(0, cut));
  morse_line_.erase(0, cut);
  if (out.empty()) return;
  print("~~ morse" + (morse_wpm_ > 0 ? " [" + std::to_string(morse_wpm_) + " wpm]" : std::string()) + ": " + out);
}

void Console::cmd_morse(const std::string& args) {
  std::string rest;
  std::string sub = first_word(args, &rest);
  std::string session;
  {
    std::lock_guard<std::mutex> lk(morse_mu_);
    session = morse_session_;
  }
  if (sub == "listen") {
    if (!session.empty()) cmd_morse("stop");
    Json sp = Json::object();
    sp.set("source", "audio");
    sp.set("sample_rate", 8000);
    Json env = call("morse.rx.start", sp);
    if (!ok(env)) return;
    const Json& sj = env.get("result").get("session");
    std::string sid = sj.is_string() ? sj.as_string() : ember_host::cfg_str(env.get("result"), "session");
    Json cp = Json::object();
    cp.set("kind", "mic_pcm");
    cp.set("session", sj);
    cp.set("sample_rate", 8000);
    if (ember_host::starts_with(rest, "file:"))
      cp.set("input_path", rest.substr(5));
    else if (!rest.empty())
      cp.set("device", rest);
    {
      std::lock_guard<std::mutex> lk(morse_mu_);
      morse_session_ = sid;
      morse_line_.clear();
      morse_wpm_ = 0;
    }
    Json c = call("host.capture.start", cp);
    if (!ok(c)) {
      Json st = Json::object();
      st.set("session", sj);
      call("morse.rx.stop", st);
      std::lock_guard<std::mutex> lk(morse_mu_);
      morse_session_.clear();
      return;
    }
    print("listening for Morse on " + (rest.empty() ? std::string("the default input") : rest) +
          " (session " + sid + ") - text appears as it is decoded; 'morse stop' to end");
    return;
  }
  if (sub == "stop") {
    if (session.empty()) {
      print("not listening (morse listen ...)");
      return;
    }
    Json cp = Json::object();
    cp.set("kind", "mic_pcm");
    call("host.capture.stop", cp);
    Json st = Json::object();
    st.set("session", session);
    Json env = call("morse.rx.stop", st);
    {
      std::lock_guard<std::mutex> lk(morse_mu_);
      morse_flush_locked(true);
      morse_session_.clear();
    }
    if (ok(env)) print("transcript: " + env.get("result").get("transcript").as_string());
    return;
  }
  if (sub == "status") {
    if (session.empty()) {
      print("not listening");
      return;
    }
    Json st = Json::object();
    st.set("session", session);
    Json env = call("morse.rx.status", st);
    if (!ok(env)) return;
    const Json& r = env.get("result");
    print(ember_host::format("morse: %s wpm=%.0f level=%.2f quality=%.2f%s pending '%s'\n  %s", r.get("on").as_bool(false) ? "MARK " : "space",
                             r.get("wpm").as_number(), r.get("level").as_number(), r.get("quality").as_number(),
                             r.get("tone_hz").is_number() ? ember_host::format(" tone=%.0fHz", r.get("tone_hz").as_number()).c_str() : "",
                             r.get("pending").as_string().c_str(), r.get("transcript").as_string().c_str()));
    return;
  }
  print("usage: morse listen [device name | file:/path/to/pcm] | morse stop | morse status");
}

void Console::cmd_call(const std::string& args) {
  std::string rest;
  std::string method = first_word(args, &rest);
  if (method.empty()) {
    print("usage: call <method> [json]");
    return;
  }
  Json p = Json::object();
  if (!rest.empty()) {
    std::string err;
    if (!Json::parse(rest, &p, &err)) {
      print("bad JSON: " + err);
      return;
    }
  }
  print(call(method, p).dump());
}

bool Console::execute(const std::string& line_in) {
  std::string rest;
  std::string cmd = ember_host::to_lower(first_word(line_in, &rest));
  if (cmd.empty()) return true;
  if (cmd == "quit" || cmd == "exit" || cmd == "q") return false;
  if (cmd == "help" || cmd == "?" || cmd == "h") {
    cmd_help();
  } else if (cmd == "info" || cmd == "whoami") {
    cmd_info();
  } else if (cmd == "peers") {
    cmd_peers();
  } else if (cmd == "contacts") {
    cmd_contacts();
  } else if (cmd == "channels") {
    cmd_channels();
  } else if (cmd == "convs" || cmd == "conversations") {
    cmd_convs();
  } else if (cmd == "read") {
    std::string n_s;
    std::string conv = resolve_conv(first_word(rest, &n_s));
    uint64_t n = 20;
    if (!n_s.empty() && !ember_host::parse_u64(n_s, &n)) n = 20;
    if (n == 0 || n > 500) n = 20;
    if (conv.empty())
      print("usage: read <conv|#n> [count]");
    else
      cmd_read(conv, int(n));
  } else if (cmd == "send") {
    std::string text;
    std::string conv = resolve_conv(first_word(rest, &text));
    cmd_send(conv, text);
  } else if (cmd == "say") {
    cmd_send(public_channel_conv(), rest);
  } else if (cmd == "dm") {
    std::string text;
    std::string who = first_word(rest, &text);
    std::string conv = resolve_conv(who);
    if (!ember_host::starts_with(conv, "dm:")) conv.clear();
    cmd_send(conv, text);
  } else if (cmd == "sos") {
    cmd_sos(rest);
  } else if (cmd == "checkin") {
    cmd_checkin(rest);
  } else if (cmd == "links") {
    cmd_links();
  } else if (cmd == "link") {
    cmd_link(rest);
  } else if (cmd == "hub") {
    cmd_hub(rest);
  } else if (cmd == "name") {
    Json p = Json::object();
    p.set("name", rest);
    if (ok(call("node.set_profile", p))) print("name set");
  } else if (cmd == "morse") {
    cmd_morse(rest);
  } else if (cmd == "unlock") {
    Json p = Json::object();
    p.set("passphrase", rest);
    Json env = call("security.unlock", p);
    if (ok(env)) print("unlocked");
  } else if (cmd == "lock") {
    if (ok(call("security.lock", Json::object()))) print("locked");
  } else if (cmd == "ping") {
    Json p = Json::object();
    p.set("node_id", rest);
    if (ok(call("mesh.ping", p))) print("ping sent");
  } else if (cmd == "events") {
    all_events_ = rest == "on";
    print(std::string("show all events: ") + (all_events_ ? "on" : "off"));
  } else if (cmd == "call") {
    cmd_call(rest);
  } else {
    print("unknown command '" + cmd + "' (try help)");
  }
  return true;
}

int Console::run(std::istream& in, const std::atomic<bool>* stop_flag) {
  print("EmberMesh console — type 'help' for commands, 'quit' to leave.");
  std::string line;
  while (!(stop_flag && *stop_flag)) {
    {
      std::lock_guard<std::mutex> lk(out_mu_);
      out_ << "> " << std::flush;
    }
    if (!std::getline(in, line)) {
      if (stop_flag && *stop_flag) break;
      if (in.eof()) break;
      in.clear();
      continue;
    }
    if (!execute(line)) break;
  }
  return 0;
}

}  // namespace emberd
