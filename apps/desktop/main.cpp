// emberd — EmberMesh desktop daemon: one self-contained executable = node + hub + radio gateway,
// plus a terminal chat client (--console in-process, --attach to a running daemon).
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "console.h"
#include "ember/ember.h"
#include "host/endpoint.h"
#include "host/host.h"
#include "host/net.h"
#include "host/util.h"
#include "remote_rpc.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char** environ;
#endif

using namespace ember_host;

namespace {

std::atomic<bool> g_stop{false};

#ifdef _WIN32
BOOL WINAPI on_ctrl(DWORD) {
  g_stop = true;
  return TRUE;
}
#else
void on_signal(int) { g_stop = true; }
#endif

void install_signals() {
#ifdef _WIN32
  SetConsoleCtrlHandler(on_ctrl, TRUE);
#else
  struct sigaction sa;
  std::memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  // no SA_RESTART: a blocking getline() returns on Ctrl-C
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGHUP, &sa, nullptr);
  std::signal(SIGPIPE, SIG_IGN);
#endif
}

const char* kUsage = R"(emberd — EmberMesh node + hub + radio gateway

Usage: emberd [options]

  --config FILE             JSON config (default <data-dir>/emberd.json if present)
  --data-dir DIR            node storage, admin token, saved links
  --web-root DIR            web app served at /
  --download-dir DIR        files served at /download/
  --port N                  hub port (default 8437)
  --bind ADDR               hub bind address (default 0.0.0.0)
  --name NAME               this node's display name (default: "EmberMesh hub" + 4 hex of the node id)
  --callsign CALL           amateur callsign (needed before kiss/audio links transmit)
  --passphrase-file FILE    unlock an encrypted store at start with the passphrase in FILE
  --no-hub                  do not serve HTTP/WebSocket (radio gateway / relay only)
  --token TOKEN             admin token for /rpc (stored in <data-dir>/admin_token)
  --new-token               generate a new admin token
  --print-token             print the admin token and exit
  --local-no-token          allow /rpc from this computer without the token
  --captive-portal          redirect OS connectivity probes to the hub page
  --no-udp                  disable the LAN multicast link (239.83.77.69:47474)
  --link JSON               bring up a link at start (host.link.create params; repeatable)
  --kiss-tcp HOST:PORT      KISS over TCP (Direwolf: 127.0.0.1:8001)
  --kiss-serial PORT[:BAUD] KISS TNC on a serial port
  --radio SPEC              sound-card radio link, SPEC = key=value,... e.g.
                            preset=handheld_vox,in=USB Audio,out=USB Audio
                            preset=handheld_ptt,in=AIOC,out=AIOC,ptt=serial_dtr,ptt_port=/dev/ttyACM0
                            keys: preset (handheld_vox|handheld_ptt|acoustic|sound_card), in, out,
                            ptt (vox|serial_rts|serial_dtr|cm108|rigctld), ptt_port, ptt_device,
                            modem, licence, rate, name   (use --callsign for amateur bands)
  --console                 interactive terminal chat (node + hub keep running)
  --attach [URL]            terminal chat against a running emberd (default ws://127.0.0.1:8437/rpc)
  --open                    open the operator UI in the default browser
  --no-qr                   do not print the QR code of the hub address
  --list-serial             list serial ports and exit
  --list-audio              list sound devices and exit
  --log-level LEVEL         debug | info | warn | error
  --log-file FILE           append logs to FILE
  --in-memory               do not persist anything (testing)
  --guides FILE             guides.pack to load (default: next to emberd, in the data dir)
  --guides-copy             read the guide pack into RAM instead of memory-mapping it
  --version, --help

Operator UI:  http://localhost:8437/?remote=1&token=<admin token>
Guests:       http://<this computer's IP>:8437/
)";

std::string default_data_dir() {
  if (const char* e = std::getenv("EMBER_DATA_DIR")) return e;
#ifdef _WIN32
  if (const char* a = std::getenv("APPDATA")) return path_join(a, "EmberMesh");
  return path_join(home_dir(), "EmberMesh");
#elif defined(__APPLE__)
  return path_join(home_dir(), "Library/Application Support/EmberMesh");
#else
  if (const char* x = std::getenv("XDG_DATA_HOME"))
    if (*x) return path_join(x, "embermesh");
  return path_join(home_dir(), ".local/share/embermesh");
#endif
}

bool stdin_is_tty() {
#ifdef _WIN32
  DWORD mode;
  return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
#else
  return isatty(0) != 0;
#endif
}

// Reads one line from the terminal without echo. Returns false on EOF / Ctrl-C.
bool read_secret(const char* prompt, std::string* out) {
  std::fputs(prompt, stderr);
  std::fflush(stderr);
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(h, &mode);
  SetConsoleMode(h, mode & ~DWORD(ENABLE_ECHO_INPUT));
  bool ok = bool(std::getline(std::cin, *out));
  SetConsoleMode(h, mode);
#else
  struct termios old_t;
  bool have = tcgetattr(0, &old_t) == 0;
  if (have) {
    struct termios t = old_t;
    t.c_lflag &= ~tcflag_t(ECHO);
    tcsetattr(0, TCSANOW, &t);
  }
  bool ok = bool(std::getline(std::cin, *out));
  if (have) tcsetattr(0, TCSANOW, &old_t);
#endif
  std::fputs("\n", stderr);
  if (!ok) std::cin.clear();
  while (!out->empty() && out->back() == '\r') out->pop_back();
  return ok;
}

bool need_arg(int i, int argc, const char* flag) {
  if (i + 1 >= argc) {
    std::fprintf(stderr, "emberd: %s needs a value\n", flag);
    return false;
  }
  return true;
}

// --radio key=value,... (or a config-file "radio" object) -> host.link.create params for a sound-card
// audio link with one of the core's handheld presets.
Json radio_link_spec(const Json& kv_in, std::string* err) {
  Json kv = Json::object();
  if (kv_in.is_string()) {
    for (const auto& part : split(kv_in.as_string(), ',')) {
      std::string t = trim(part);
      if (t.empty()) continue;
      size_t eq = t.find('=');
      if (eq == std::string::npos) {
        *err = "expected key=value, got '" + t + "'";
        return Json();
      }
      kv.set(trim(t.substr(0, eq)), trim(t.substr(eq + 1)));
    }
  } else if (kv_in.is_object()) {
    kv = kv_in;
  }
  static const char* known[] = {"preset", "in", "out", "ptt", "ptt_port", "ptt_device", "ptt_gpio", "modem", "licence", "rate", "name", "tx_gain_db", "rx_gain_db"};
  for (const auto& m : kv.items()) {
    bool ok = false;
    for (const char* k : known) ok |= m.first == k;
    if (!ok) {
      *err = "unknown key '" + m.first + "'";
      return Json();
    }
  }
  std::string preset = cfg_str(kv, "preset", "handheld_vox");
  std::string ptt = cfg_str(kv, "ptt", preset == "handheld_ptt" || preset == "sound_card" ? "serial_rts" : "vox");
  Json ep = Json::object();
  ep.set("input_device", cfg_str(kv, "in"));
  ep.set("output_device", cfg_str(kv, "out"));
  ep.set("sample_rate", double(cfg_int(kv, "rate", 48000)));
  ep.set("ptt", ptt);
  if (kv.has("ptt_port")) ep.set("ptt_port", cfg_str(kv, "ptt_port"));
  if (kv.has("ptt_device")) ep.set("ptt_device", cfg_str(kv, "ptt_device"));
  if (kv.has("ptt_gpio")) ep.set("ptt_gpio", double(cfg_int(kv, "ptt_gpio", 3)));
  if (kv.has("tx_gain_db")) ep.set("tx_gain_db", cfg_num(kv, "tx_gain_db", 0));
  if (kv.has("rx_gain_db")) ep.set("rx_gain_db", cfg_num(kv, "rx_gain_db", 0));
  if ((ptt == "serial_rts" || ptt == "serial_dtr") && !kv.has("ptt_port")) {
    *err = "ptt=" + ptt + " needs ptt_port=/dev/... (emberd --list-serial)";
    return Json();
  }
  Json lc = Json::object();
  lc.set("preset", preset);
  if (kv.has("modem")) lc.set("modem", cfg_str(kv, "modem"));
  if (kv.has("licence")) lc.set("licence", cfg_str(kv, "licence"));
  Json spec = Json::object();
  spec.set("name", cfg_str(kv, "name", "Radio (" + preset + ")"));
  Json e = Json::object();
  e.set("kind", "audio");
  e.set("config", ep);
  spec.set("endpoint", e);
  Json l = Json::object();
  l.set("type", "audio");
  l.set("config", lc);
  spec.set("link", l);
  return spec;
}

// Parses CLI flags into a JSON object with the same keys as the config file.
bool parse_cli(int argc, char** argv, Json* o) {
  *o = Json::object();
  Json links = Json::array();
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto val = [&](const char* key) {
      if (!need_arg(i, argc, a.c_str())) return false;
      o->set(key, argv[++i]);
      return true;
    };
    if (a == "--help" || a == "-h") {
      std::fputs(kUsage, stdout);
      std::exit(0);
    } else if (a == "--version" || a == "-V") {
      std::printf("emberd %s (core %s)\n", EMBER_HOST_VERSION, ember_version());
      std::exit(0);
    } else if (a == "--config") {
      if (!val("config")) return false;
    } else if (a == "--data-dir") {
      if (!val("data_dir")) return false;
    } else if (a == "--web-root") {
      if (!val("web_root")) return false;
    } else if (a == "--download-dir") {
      if (!val("download_dir")) return false;
    } else if (a == "--bind") {
      if (!val("bind")) return false;
    } else if (a == "--name") {
      if (!val("name")) return false;
    } else if (a == "--callsign") {
      if (!val("callsign")) return false;
    } else if (a == "--passphrase-file") {
      if (!val("passphrase_file")) return false;
    } else if (a == "--token") {
      if (!val("token")) return false;
    } else if (a == "--log-level") {
      if (!val("log_level")) return false;
    } else if (a == "--log-file") {
      if (!val("log_file")) return false;
    } else if (a == "--guides") {
      if (!val("guides")) return false;
    } else if (a == "--port") {
      if (!need_arg(i, argc, "--port")) return false;
      uint64_t p;
      if (!parse_u64(argv[++i], &p) || p > 65535) {
        std::fprintf(stderr, "emberd: bad port '%s'\n", argv[i]);
        return false;
      }
      o->set("port", double(p));
    } else if (a == "--no-hub") {
      o->set("hub", false);
    } else if (a == "--new-token") {
      o->set("new_token", true);
    } else if (a == "--print-token") {
      o->set("print_token", true);
    } else if (a == "--local-no-token") {
      o->set("local_no_token", true);
    } else if (a == "--no-udp") {
      o->set("udp", false);
    } else if (a == "--console") {
      o->set("console", true);
    } else if (a == "--attach") {
      o->set("attach", true);
      if (i + 1 < argc && argv[i + 1][0] != '-') o->set("attach_url", argv[++i]);
    } else if (a == "--open") {
      o->set("open", true);
    } else if (a == "--guides-copy") {
      o->set("guides_copy", true);
    } else if (a == "--no-qr") {
      o->set("no_qr", true);
    } else if (a == "--list-serial") {
      o->set("list_serial", true);
    } else if (a == "--list-audio") {
      o->set("list_audio", true);
    } else if (a == "--in-memory") {
      o->set("in_memory", true);
    } else if (a == "--captive-portal") {
      o->set("captive_portal", true);
    } else if (a == "--link") {
      if (!need_arg(i, argc, "--link")) return false;
      Json l;
      std::string err;
      if (!Json::parse(argv[++i], &l, &err) || !l.is_object()) {
        std::fprintf(stderr, "emberd: --link: bad JSON: %s\n", err.c_str());
        return false;
      }
      links.push(l);
    } else if (a == "--kiss-tcp") {
      if (!need_arg(i, argc, "--kiss-tcp")) return false;
      std::string v = argv[++i];
      size_t c = v.rfind(':');
      uint64_t port = 8001;
      std::string host = v;
      if (c != std::string::npos) {
        host = v.substr(0, c);
        if (!parse_u64(v.substr(c + 1), &port) || port == 0 || port > 65535) {
          std::fprintf(stderr, "emberd: --kiss-tcp: bad port in '%s'\n", v.c_str());
          return false;
        }
      }
      Json l = Json::parse_or_null(format(R"({"name":"KISS TCP %s","endpoint":{"kind":"tcp","config":{"host":%s,"port":%llu}},"link":{"type":"kiss","config":{}}})",
                                          v.c_str(), json_quote(host).c_str(), static_cast<unsigned long long>(port)));
      links.push(l);
    } else if (a == "--radio") {
      if (!need_arg(i, argc, "--radio")) return false;
      std::string err;
      Json l = radio_link_spec(argv[++i], &err);
      if (!err.empty()) {
        std::fprintf(stderr, "emberd: --radio: %s\n", err.c_str());
        return false;
      }
      links.push(l);
    } else if (a == "--kiss-serial") {
      if (!need_arg(i, argc, "--kiss-serial")) return false;
      std::string v = argv[++i];
      uint64_t baud = 9600;
      std::string port = v;
      size_t c = v.rfind(':');
      if (c != std::string::npos && c > 1) {  // keep "COM3" / "/dev/x" intact; "C:" is not a baud
        uint64_t b;
        if (parse_u64(v.substr(c + 1), &b)) {
          baud = b;
          port = v.substr(0, c);
        }
      }
      Json l = Json::parse_or_null(format(R"({"name":"KISS serial %s","endpoint":{"kind":"serial","config":{"port":%s,"baud":%llu}},"link":{"type":"kiss","config":{}}})",
                                          port.c_str(), json_quote(port).c_str(), static_cast<unsigned long long>(baud)));
      links.push(l);
    } else {
      std::fprintf(stderr, "emberd: unknown option '%s' (see --help)\n", a.c_str());
      return false;
    }
  }
  if (links.size()) o->set("links", links);
  return true;
}

// A web root guests can actually run: the UI plus the WebAssembly core (dist/web from
// ./build.sh web). The plain ui/ folder has no wasm/ and guests would see "could not start".
bool has_wasm(const std::string& d) { return is_file(path_join(d, "wasm/ember.wasm")); }

std::string find_web_root(const std::string& configured) {
  if (!configured.empty()) return configured;
  std::vector<std::string> c;
  if (const char* e = std::getenv("EMBER_WEB_ROOT")) c.push_back(e);
  std::string exe = executable_dir();
  c.push_back("dist/web");                            // ./build.sh web output, from the repo root
  c.push_back(path_join(exe, "../share/embermesh/web"));  // install.sh layout
  c.push_back(path_join(exe, "web"));                 // dist/desktop/web next to the binary
  c.push_back(path_join(exe, "../web"));              // dist/desktop/emberd -> dist/web
  c.push_back(path_join(exe, "../../../dist/web"));   // build/<dir>/apps/desktop/emberd -> repo
  c.push_back(path_join(exe, "../../../../dist/web"));
  // No realpath(): it touches every parent directory (can block on macOS privacy prompts).
  for (const auto& d : c)
    if (is_file(path_join(d, "index.html")) && has_wasm(d)) return d;
  // Last resort: a UI without the core (guests cannot run their own node) beats no UI at all.
  for (const auto& d : {std::string("ui"), path_join(exe, "../share/embermesh/ui"), path_join(exe, "ui"),
                        path_join(exe, "../../../ui")})
    if (is_file(path_join(d, "index.html"))) return d;
  return std::string();
}

// guides.pack lookup: --guides, next to the binary, the data dir, the install layout, the repo.
std::vector<std::string> guide_pack_candidates(const Json& opts, const std::string& data_dir, const std::string& web_root) {
  std::vector<std::string> c;
  if (opts.get("guides").is_string()) c.push_back(opts.get("guides").as_string());
  std::string exe = executable_dir();
  c.push_back(path_join(exe, "guides.pack"));                       // dist/desktop/guides.pack
  c.push_back(path_join(data_dir, "guides.pack"));
  c.push_back(path_join(exe, "../share/embermesh/guides.pack"));   // install.sh layout
  if (!web_root.empty()) c.push_back(path_join(web_root, "guides.pack"));
  c.push_back("build/guides.pack");                                 // ./build.sh guides, repo root
  c.push_back(path_join(exe, "../../../guides.pack"));              // build/<dir>/apps/desktop -> build/
  return c;
}

void print_qr(Host& host, const std::string& text) {
  // Hook: uses the core's QR encoder (tools.qr) once it exists; silently skipped otherwise.
  Json p = Json::object();
  p.set("text", text);
  p.set("ecc", "L");
  Json env = host.call("tools.qr", p);
  if (!env.get("ok").as_bool(false)) return;
  const Json& rows = env.get("result").get("rows");
  size_t n = rows.size();
  if (n == 0 || n > 177) return;
  auto dark = [&](long y, long x) -> bool {
    if (y < 0 || x < 0 || size_t(y) >= n) return false;
    const std::string& r = rows.at(size_t(y)).as_string();
    return size_t(x) < r.size() && r[size_t(x)] == '1';
  };
  // Light modules are drawn as blocks so the code reads correctly on a dark terminal.
  const long q = 2;
  std::string out;
  for (long y = -q; y < long(n) + q; y += 2) {
    out += "  ";
    for (long x = -q; x < long(n) + q; ++x) {
      bool top_light = !dark(y, x), bot_light = (y + 1 < long(n) + q) ? !dark(y + 1, x) : false;
      out += top_light && bot_light ? "\xE2\x96\x88" : top_light ? "\xE2\x96\x80" : bot_light ? "\xE2\x96\x84" : " ";
    }
    out += "\n";
  }
  std::fputs(out.c_str(), stdout);
}

void open_browser(const std::string& url) {
#ifdef _WIN32
  ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#ifdef __APPLE__
  std::vector<std::string> args = {"open", url};
#else
  std::vector<std::string> args = {"xdg-open", url};
#endif
  std::vector<char*> argv;
  for (auto& a : args) argv.push_back(&a[0]);
  argv.push_back(nullptr);
  pid_t pid;
  if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) == 0) {
    std::thread([pid] {
      int st;
      waitpid(pid, &st, 0);
    }).detach();
  } else {
    std::fprintf(stderr, "emberd: could not start a browser; open %s yourself\n", url.c_str());
  }
#endif
}

int list_devices(const char* kind) {
  EndpointRegistry reg = make_default_registry();
  const EndpointKind* k = reg.find(kind);
  if (!k || !k->available || !k->scan) {
    std::printf("%s: not available in this build%s%s\n", kind, k ? ": " : "", k ? k->unavailable_reason.c_str() : "");
    return 1;
  }
  Json p = Json::object();
  p.set("kind", kind);
  Json r = k->scan(p);
  const Json& devs = r.get("devices");
  if (!devs.size()) std::printf("(no %s devices found)\n", kind);
  for (const auto& d : devs.elements())
    std::printf("  %-40s %s%s%s\n", d.get("id").as_string().c_str(), d.get("name").as_string().c_str(),
                d.get("detail").as_string().empty() ? "" : "  — ", d.get("detail").as_string().c_str());
  return 0;
}

std::string load_or_make_token(const std::string& data_dir, const std::string& given, bool regenerate, bool persist) {
  std::string path = path_join(data_dir, "admin_token");
  if (!given.empty()) {
    if (persist) write_file_atomic(path, given + "\n", true);
    return given;
  }
  if (persist && !regenerate) {
    std::string t;
    if (read_file(path, &t, 4096)) {
      t = trim(t);
      if (t.size() >= 16) return t;
    }
  }
  std::string t = random_hex(16);
  if (persist && !write_file_atomic(path, t + "\n", true)) EH_LOGW("cannot store admin token in %s", path.c_str());
  return t;
}

int run_attach(const Json& opts, const std::string& data_dir) {
  std::string url = opts.get("attach_url").as_string();
  uint16_t port = uint16_t(cfg_int(opts, "port", 8437));
  if (url.empty()) url = format("ws://127.0.0.1:%u/rpc", port);
  if (url.find("token=") == std::string::npos) {
    std::string t = opts.get("token").as_string();
    if (t.empty()) {
      read_file(path_join(data_dir, "admin_token"), &t, 4096);
      t = trim(t);
    }
    if (!t.empty()) url += (url.find('?') == std::string::npos ? "?token=" : "&token=") + t;
  }
  emberd::RemoteRpc rpc;
  std::string err;
  if (!rpc.connect(url, &err)) {
    std::fprintf(stderr, "emberd: cannot attach to %s: %s\n", url.substr(0, url.find("token=")).c_str(), err.c_str());
    return 1;
  }
  emberd::Console con([&rpc](const std::string& m, const Json& p) { return rpc.call(m, p); }, std::cout);
  rpc.on_event = [&con](const std::string& e) { con.on_event(e); };
  rpc.on_disconnect = [](const std::string& why) {
    std::fprintf(stderr, "\nemberd: disconnected (%s)\n", why.c_str());
    g_stop = true;
  };
  con.execute("info");
  return con.run(std::cin, &g_stop);
}

}  // namespace

int main(int argc, char** argv) {
  net_init();
  Json cli;
  if (!parse_cli(argc, argv, &cli)) return 2;

  // defaults < config file < command line
  std::string data_dir = cli.get("data_dir").as_string(default_data_dir());
  std::string config_path = cli.get("config").as_string(path_join(data_dir, "emberd.json"));
  Json opts = Json::object();
  {
    std::string text;
    if (read_file(config_path, &text, 1 << 20)) {
      std::string err;
      if (!Json::parse(text, &opts, &err) || !opts.is_object()) {
        std::fprintf(stderr, "emberd: %s: %s\n", config_path.c_str(), err.c_str());
        return 2;
      }
    } else if (cli.has("config")) {
      std::fprintf(stderr, "emberd: cannot read config %s\n", config_path.c_str());
      return 2;
    }
  }
  Json file_links = opts.get("links");
  if (opts.has("radio")) {  // config file: "radio": {"preset":"handheld_vox","in":"USB Audio",...} or "k=v,..."
    std::string rerr;
    Json rl = radio_link_spec(opts.get("radio"), &rerr);
    if (!rerr.empty()) {
      std::fprintf(stderr, "emberd: %s: radio: %s\n", config_path.c_str(), rerr.c_str());
      return 2;
    }
    Json fl = Json::array();
    for (const auto& l : file_links.elements()) fl.push(l);
    fl.push(rl);
    file_links = fl;
  }
  for (const auto& kv : cli.items()) opts.set(kv.first, kv.second);
  Json links = Json::array();
  for (const auto& l : file_links.elements()) links.push(l);
  for (const auto& l : cli.get("links").elements()) links.push(l);
  if (opts.has("data_dir")) data_dir = opts.get("data_dir").as_string(data_dir);

  set_log_level(parse_log_level(opts.get("log_level").as_string(opts.get("console").as_bool(false) ? "warn" : "info")));
  std::FILE* log_fp = nullptr;
  if (opts.get("log_file").is_string()) {
    log_fp = fopen_utf8(opts.get("log_file").as_string(), "ab");
    if (!log_fp) {
      std::fprintf(stderr, "emberd: cannot open log file %s\n", opts.get("log_file").as_string().c_str());
      return 2;
    }
    set_log_sink([log_fp](LogLevel l, const char* line) {
      static const char* names[] = {"debug", "info", "warn", "error", "off"};
      std::fprintf(log_fp, "%llu [%s] %s\n", static_cast<unsigned long long>(unix_ms() / 1000), names[int(l)], line);
      std::fflush(log_fp);
    });
  }

  if (opts.get("list_serial").as_bool(false)) return list_devices("serial");
  if (opts.get("list_audio").as_bool(false)) return list_devices("audio");

  install_signals();
  if (opts.get("attach").as_bool(false)) return run_attach(opts, data_dir);

  bool in_memory = opts.get("in_memory").as_bool(false);
  if (!in_memory && !make_dirs(data_dir)) {
    std::fprintf(stderr, "emberd: cannot create data directory %s\n", data_dir.c_str());
    return 1;
  }
  std::string token = load_or_make_token(data_dir, opts.get("token").as_string(), opts.get("new_token").as_bool(false), !in_memory);
  if (opts.get("print_token").as_bool(false)) {
    std::printf("%s\n", token.c_str());
    return 0;
  }

  // ---- node ----
  uint8_t entropy[64];
  if (!random_bytes(entropy, sizeof entropy)) {
    std::fprintf(stderr, "emberd: no OS randomness available\n");
    return 1;
  }
  // Privacy: never announce the computer's user or host name. Without --name the node is called
  // "EmberMesh hub <first 4 hex of its node id>" (set below once the identity exists).
  std::string name = opts.get("name").as_string("EmberMesh hub");
  Json ncfg = Json::object();
  ncfg.set("storage_dir", in_memory ? std::string() : path_join(data_dir, "node"));
  if (!in_memory) make_dirs(path_join(data_dir, "node"));
  ncfg.set("platform", "desktop");
  ncfg.set("app_version", EMBER_HOST_VERSION);
  ncfg.set("device_name", name);
  ncfg.set("now_ms", double(unix_ms()));  // otherwise sends are `busy` until the first tick
  ember_node* node = ember_create(ncfg.dump().c_str(), entropy, sizeof entropy);
  std::memset(entropy, 0, sizeof entropy);
  if (!node) {
    std::fprintf(stderr, "emberd: could not create the node\n");
    return 1;
  }

  // ---- host config ----
  HostConfig hc;
  hc.data_dir = in_memory ? std::string() : data_dir;
  hc.web_root = find_web_root(opts.get("web_root").as_string());

  // The guide library (tens of MB) is memory-mapped and handed to the core zero-copy: pages load on
  // demand and are shared with the OS page cache. Fallback: read it into memory (--guides-copy, or
  // if mapping fails).
  MappedFile guides_map;
  std::string guides_data;
  std::string guides_path;
  size_t guides_size = 0;
  bool guides_copy = opts.get("guides_copy").as_bool(false);
  for (const auto& gp : guide_pack_candidates(opts, data_dir, hc.web_root)) {
    if (!is_file(gp)) continue;
    const uint8_t* ptr = nullptr;
    size_t len = 0;
    std::string merr;
    if (!guides_copy && guides_map.open(gp, &merr)) {
      ptr = guides_map.data();
      len = guides_map.size();
    } else if (read_file(gp, &guides_data, size_t(1) << 30)) {
      ptr = reinterpret_cast<const uint8_t*>(guides_data.data());
      len = guides_data.size();
    } else {
      continue;
    }
    int grc = ember_guides_load(node, ptr, len, 0);
    if (grc == 0) {
      guides_path = gp;
      guides_size = len;
      EH_LOGI("guides: loaded %s (%zu KiB, %s)", gp.c_str(), len / 1024, guides_map.is_open() ? "memory-mapped" : "copied");
      break;
    }
    EH_LOGW("guides: %s is not a valid guides.pack (%d)", gp.c_str(), grc);
    guides_map.close();
    guides_data.clear();
  }
  if (guides_path.empty())
    EH_LOGW("guides: no guides.pack found (use --guides FILE; build it with ./build.sh guides): the library is empty");

  if (opts.get("download_dir").is_string()) hc.download_dirs.push_back(opts.get("download_dir").as_string());
  if (!in_memory) {
    std::string dd = path_join(data_dir, "download");
    make_dirs(dd);
    hc.download_dirs.push_back(dd);
  }
  // Files for spreading the app: the single-file build (dist/web/embermesh.html) and dist/ (APK...).
  if (!hc.web_root.empty() && is_file(path_join(hc.web_root, "embermesh.html")))
    hc.download_files.push_back(path_join(hc.web_root, "embermesh.html"));
  else if (is_file("dist/web/embermesh.html"))
    hc.download_files.push_back("dist/web/embermesh.html");
  if (is_dir("dist/android")) hc.download_dirs.push_back("dist/android");
  hc.admin_token = token;
  hc.local_no_token = opts.get("local_no_token").as_bool(false);
  hc.captive_portal = opts.get("captive_portal").as_bool(false);
  hc.hub_autostart = opts.get("hub").as_bool(true);
  hc.hub_restore = hc.hub_autostart;  // --no-hub also keeps a saved hub link down
  hc.hub_port = uint16_t(cfg_int(opts, "port", 8437));
  hc.hub_bind = opts.get("bind").as_string("0.0.0.0");
  if (hc.web_root.empty() && hc.hub_autostart)
    EH_LOGW("no web root found (use --web-root DIR): serving the built-in fallback page");
  else if (hc.hub_autostart && !has_wasm(hc.web_root))
    EH_LOGW("web root %s has no wasm/ember.wasm: guests will see \"EmberMesh could not start\". "
            "Build the web app (./build.sh web) and use --web-root dist/web",
            hc.web_root.c_str());
  // Links for this run (CLI / config file): the core keeps them only while emberd runs
  // (persist:false); links added live from the operator page or console are persisted by the core.
  if (opts.get("udp").as_bool(true))
    hc.startup_links.push(Json::parse_or_null(
        R"({"name":"LAN multicast","endpoint":{"kind":"udp","config":{}},"link":{"type":"raw","config":{"preset":"udp_lan"}},"host_key":"cfg-udp"})"));
  for (const auto& l : links.elements()) hc.startup_links.push(l);

  // Profile from --name / --callsign before any link exists, so kiss/audio links are created with the
  // callsign. A fresh identity gets the neutral name "EmberMesh hub xxxx" (never the computer's name).
  auto apply_profile = [&](const std::function<Json(const std::string&, const Json&)>& call) {
    Json ni0 = call("node.info", Json::object()).get("result");
    if (ni0.get("locked").as_bool(false)) return false;
    Json prof = Json::object();
    if (opts.get("name").is_string() && ni0.get("name").as_string() != opts.get("name").as_string())
      prof.set("name", opts.get("name").as_string());
    else if (ni0.get("name").as_string().empty())
      prof.set("name", "EmberMesh hub " + ni0.get("node_id").as_string().substr(0, 4));
    if (opts.get("callsign").is_string() && ni0.get("callsign").as_string() != opts.get("callsign").as_string())
      prof.set("callsign", opts.get("callsign").as_string());
    if (prof.size()) {
      Json r = call("node.set_profile", prof);
      if (!r.get("ok").as_bool(false)) EH_LOGW("profile: %s", r.get("error").get("message").as_string().c_str());
    }
    return true;
  };
  auto direct_call = [node](const std::string& m, const Json& p) {
    char* r = ember_call(node, m.c_str(), p.dump().c_str());
    Json j = Json::parse_or_null(r ? r : "{}");
    ember_free(r);
    return j;
  };
  bool locked_at_start = !apply_profile(direct_call);

  auto host = std::make_unique<Host>(node, hc);
  host->start_thread();
  host->start();

  // ---- locked store ----
  auto is_locked = [&]() { return host->call("node.info", Json::object()).get("result").get("locked").as_bool(false); };
  auto try_unlock = [&](const std::string& pass) {
    Json p = Json::object();
    p.set("passphrase", pass);
    Json r = host->call("security.unlock", p);
    return r.get("ok").as_bool(false) && r.get("result").get("unlocked").as_bool(true);
  };
  if (is_locked()) {
    bool done = false;
    if (opts.get("passphrase_file").is_string()) {
      std::string pass;
      if (!read_file(opts.get("passphrase_file").as_string(), &pass, 4096)) {
        std::fprintf(stderr, "emberd: cannot read passphrase file %s\n", opts.get("passphrase_file").as_string().c_str());
      } else {
        while (!pass.empty() && (pass.back() == '\n' || pass.back() == '\r')) pass.pop_back();
        done = try_unlock(pass);
        std::fprintf(stderr, done ? "emberd: storage unlocked\n" : "emberd: wrong passphrase in %s\n",
                     opts.get("passphrase_file").as_string().c_str());
      }
    }
    if (!done && stdin_is_tty() && !opts.get("attach").as_bool(false)) {
      for (int attempt = 0; attempt < 3 && !done && !g_stop; ++attempt) {
        std::string pass;
        if (!read_secret("emberd: storage is locked. Passphrase (empty = unlock later from the operator page): ", &pass) ||
            pass.empty())
          break;
        done = try_unlock(pass);
        std::fprintf(stderr, done ? "emberd: storage unlocked\n" : "emberd: wrong passphrase\n");
      }
    }
    if (!done)
      std::fprintf(stderr,
                   "emberd: storage is LOCKED. The hub serves the app, but this node joins the mesh only after\n"
                   "        unlocking: operator page (Settings > Security), console 'unlock', or --passphrase-file.\n");
  }

  if (locked_at_start && !is_locked()) apply_profile([&](const std::string& m, const Json& p) { return host->call(m, p); });

  // ---- banner ----
  Json info = host->call("node.info", Json::object());
  const Json& ni = info.get("result");
  std::printf("EmberMesh emberd %s (core %s) — node %s \"%s\"%s\n", EMBER_HOST_VERSION, ember_version(),
              ni.get("node_id").as_string().empty() ? "(locked)" : ni.get("node_id").as_string().c_str(),
              ni.get("name").as_string().c_str(), ni.get("locked").as_bool(false) ? " [LOCKED]" : "");
  std::printf("  data dir:  %s\n", in_memory ? "(in memory)" : data_dir.c_str());
  std::printf("  web root:  %s\n", hc.web_root.empty() ? "(built-in fallback page)" : hc.web_root.c_str());
  std::printf("  guides:    %s\n", guides_path.empty() ? "(none loaded)" : format("%s (%.1f MB)", guides_path.c_str(), double(guides_size) / 1e6).c_str());
  Json hub = host->call("host.hub.status", Json::object());
  const Json& hs = hub.get("result");
  if (hs.get("running").as_bool(false)) {
    std::printf("  Hub is running. Phones and laptops on this network open:\n");
    for (const auto& u : hs.get("urls").elements()) std::printf("      %s\n", u.as_string().c_str());
    uint16_t port = uint16_t(hs.get("port").as_int(8437));
    std::string remote = format("http://localhost:%u/?remote=1&token=%s", port, token.c_str());
    std::printf("  Operator (control this node from a browser on this computer):\n      %s\n", remote.c_str());
    std::printf("  Terminal chat: emberd --attach%s\n",
                port == 8437 ? "" : format(" ws://127.0.0.1:%u/rpc", unsigned(port)).c_str());
    if (hs.get("urls").size() && !opts.get("no_qr").as_bool(false)) {
      std::printf("  Scan to open %s :\n", hs.get("urls").at(0).as_string().c_str());
      print_qr(*host, hs.get("urls").at(0).as_string());
    }
    if (opts.get("open").as_bool(false)) open_browser(remote);
  } else if (hc.hub_autostart) {
    std::printf("  Hub could not start (see log above; is port %u in use?)\n", unsigned(hc.hub_port));
  } else {
    std::printf("  Hub disabled (--no-hub).\n");
  }
  std::fflush(stdout);

  int rc = 0;
  if (opts.get("console").as_bool(false)) {
    emberd::Console con([&host](const std::string& m, const Json& p) { return host->call(m, p); }, std::cout);
    int lid = -1;
    host->loop().post([&] { lid = host->add_event_listener([&con](const std::string& e) { con.on_event(e); }); });
    con.execute("info");
    rc = con.run(std::cin, &g_stop);
    host->loop().post([&host, lid] { host->remove_event_listener(lid); });
  } else {
    while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::printf("\nemberd: shutting down\n");
  host->stop_thread();
  host.reset();
  ember_destroy(node);
  set_log_sink(nullptr);
  if (log_fp) std::fclose(log_fp);
  return rc;
}
