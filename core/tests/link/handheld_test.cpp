// Audio-link presets through realistic radio paths (dsp/radio_channel.h): a VOX-keyed handheld, a
// PTT-keyed handheld via an interface, a mobile with a data port, and acoustic coupling. Each preset
// must deliver frames reliably through its matching model; the default (non-preset) settings are
// shown to fail where the preset succeeds. Measured rates are printed (MESSAGE) and summarised in
// core/src/link/README.md "Handheld radios".
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "dsp/radio_channel.h"
#include "link_harness.h"
#include "rf/radios.h"

using namespace ember;
using namespace linktest;

namespace {

constexpr uint32_t kRate = 16000;

enum class Model { Vox, Ptt, DataPort, Acoustic };

dsp::RadioPathParams model_params(Model m, uint64_t seed) {
  dsp::RadioPathParams p;
  p.seed = seed;
  switch (m) {
    case Model::Vox:  // cheap handheld keyed by VOX from a K1 cable
      p.keying = dsp::Keying::Vox;
      p.vox_threshold = 0.02;
      p.vox_attack_ms = 300 + static_cast<double>(seed % 4) * 100;  // 300..600 ms
      p.vox_hang_ms = 500;
      p.mic_gain = 2.0;   // mic input far more sensitive than line out
      p.squelch_open_ms = 150;
      p.squelch_tail_ms = 200;
      p.fm_snr_db = 22;
      break;
    case Model::Ptt:  // same handheld, PTT keyed by an interface
      p.keying = dsp::Keying::Ptt;
      p.ptt_delay_ms = 150;      // transmitter key-up
      p.squelch_open_ms = 250;   // cheap receiver with a slow squelch
      p.squelch_tail_ms = 200;
      p.mic_gain = 2.0;
      p.fm_snr_db = 22;
      break;
    case Model::DataPort:  // mobile radio, data port: fast keying, flat audio
      p.keying = dsp::Keying::Ptt;
      p.ptt_delay_ms = 60;
      p.squelch_open_ms = 80;
      p.squelch_tail_ms = 120;
      p.preemphasis = false;
      p.deemphasis = false;
      p.rx_agc = false;
      p.mic_gain = 1.0;
      p.fm_snr_db = 25;
      break;
    case Model::Acoustic:  // phone held to a handheld, PTT pressed by hand
      p.keying = dsp::Keying::Manual;
      p.ptt_delay_ms = 500 + static_cast<double>(seed % 3) * 200;  // operator reaction 0.5..0.9 s
      p.ptt_release_ms = 150;
      p.acoustic = true;
      p.reverb_mix = 0.35;
      p.room_noise = 0.02;
      p.squelch_open_ms = 150;
      p.squelch_tail_ms = 200;
      p.mic_gain = 1.0;
      p.fm_snr_db = 20;
      break;
  }
  return p;
}

struct Result {
  size_t sent = 0, got = 0;
  uint32_t key_ups = 0;
  double rate() const { return sent ? static_cast<double>(got) / static_cast<double>(sent) : 0; }
};

// Sends `bursts` x `per_burst` frames from a link configured `cfg` through model `m`, received by a
// link with the same modem. Returns frames delivered / sent.
Result run_case(const std::string& cfg_tx, const std::string& cfg_rx, Model m, int bursts, int per_burst,
                size_t frame_len, uint64_t seed0) {
  Result res;
  for (int b = 0; b < bursts; ++b) {
    uint64_t seed = seed0 + static_cast<uint64_t>(b);
    Clock clk;
    FakeHost ht(&clk, 100 + seed), hr(&clk, 200 + seed);
    std::string err;
    auto tx = create_link("audio", cfg_tx, ht, &err);
    REQUIRE_MESSAGE(tx, err);
    auto rx = create_link("audio", cfg_rx, hr, &err);
    REQUIRE_MESSAGE(rx, err);
    tx->on_io_state(true);
    rx->on_io_state(true);
    std::set<Bytes> want;
    for (int k = 0; k < per_burst; ++k) {
      Bytes f(frame_len);
      for (size_t i = 0; i < f.size(); ++i) f[i] = static_cast<uint8_t>(seed * 31 + static_cast<uint64_t>(k) * 7 + i * 13);
      REQUIRE(tx->send(ByteView(f), 1));
      want.insert(f);
    }
    // Capture everything the transmitter produces, with the PTT line, until its queue is empty.
    std::vector<float> audio;
    std::vector<uint8_t> ptt;
    const size_t blk = kRate / 100;
    std::vector<int16_t> buf(blk);
    bool line = false;
    size_t ev = 0;
    int idle_after = 0;
    for (int t = 0; t < 60000 && idle_after < 50; ++t) {  // 10 ms steps, <= 10 min
      clk.t += 10;
      tx->tick(clk.t);
      size_t n = tx->pull_audio(buf.data(), blk, kRate);
      for (; ev < ht.events.size(); ++ev)
        if (ht.events[ev].first == "ptt") line = ht.events[ev].second.find("true") != std::string::npos;
      for (size_t i = 0; i < blk; ++i) {
        audio.push_back(i < n ? buf[i] / 32768.0f : 0.0f);
        ptt.push_back(line ? 1 : 0);
      }
      bool busy = tx->tx_queue_depth() > 0 || tx->channel_busy() || n > 0;
      idle_after = busy ? 0 : idle_after + 1;
    }
    dsp::RadioPath path(model_params(m, seed), kRate);
    std::vector<float> heard = path.run(audio, ptt, kRate);
    res.key_ups += path.stats().key_ups;
    std::vector<int16_t> pcm(heard.size());
    for (size_t i = 0; i < heard.size(); ++i) pcm[i] = static_cast<int16_t>(heard[i] * 32767.0f);
    for (size_t off = 0; off < pcm.size(); off += blk) {
      size_t n = std::min(blk, pcm.size() - off);
      rx->on_audio(pcm.data() + off, n, kRate);
      clk.t += 10;
      rx->tick(clk.t);
    }
    std::set<Bytes> got;
    for (const auto& f : hr.frames)
      if (want.count(f.data)) got.insert(f.data);
    res.sent += want.size();
    res.got += got.size();
  }
  return res;
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("presets: links.types lists the audio preset field and explicit fields win") {
  Json types = parse_json(link_types_json());
  bool found = false;
  for (const Json& t : types.items()) {
    if (t.get_string("type") != "audio") continue;
    for (const Json& f : t.get("config").items()) {
      if (f.get_string("key") != "preset") continue;
      found = true;
      std::set<std::string> ids;
      for (const Json& o : f.get("options").items()) ids.insert(o.get_string("value"));
      CHECK(ids == std::set<std::string>{"handheld_vox", "handheld_ptt", "acoustic", "sound_card", "custom"});
      CHECK(f.get_string("default") == "custom");
    }
  }
  CHECK(found);
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto a = create_link("audio", R"({"preset":"acoustic"})", h, &err);
  REQUIRE(a);
  Json st = parse_json(a->status_json());
  CHECK(st.get_string("preset") == "acoustic");
  CHECK(st.get("modem_status").get_string("modem") == "robust125");
  auto b = create_link("audio", R"({"preset":"acoustic","modem":"robust250"})", h, &err);
  REQUIRE(b);
  CHECK(parse_json(b->status_json()).get("modem_status").get_string("modem") == "robust250");
  // reconfigure applies presets the same way
  REQUIRE(b->reconfigure(R"({"preset":"handheld_vox"})", &err));
  CHECK(parse_json(b->status_json()).get("modem_status").get_string("modem") == "afsk1200");
  // custom == the historical defaults
  auto c = create_link("audio", "{}", h, &err);
  REQUIRE(c);
  CHECK(parse_json(c->status_json()).get_string("preset") == "custom");
}

TEST_CASE("presets: each preset delivers through its radio model; defaults fail where presets succeed") {
  const std::string lf = R"("licence":"licence_free")";
  struct Row {
    const char* name;
    Model model;
    std::string preset_cfg, default_cfg;
    int bursts, per_burst;
    size_t len;
    double preset_min, default_max;  // required preset success; default must be below this
  };
  // Single-message transmissions (the common case: one chat message = one key-up).
  const Row rows[] = {
      {"handheld_vox via VOX handheld", Model::Vox, "{\"preset\":\"handheld_vox\"," + lf + "}", "{" + lf + "}", 8, 1,
       80, 0.9, 0.5},
      {"handheld_ptt via PTT handheld", Model::Ptt, "{\"preset\":\"handheld_ptt\"," + lf + "}", "{" + lf + "}", 8, 1,
       80, 0.9, 0.5},
      {"acoustic via acoustic coupling", Model::Acoustic, "{\"preset\":\"acoustic\"," + lf + "}", "{" + lf + "}", 3,
       1, 40, 0.65, 0.5},
      {"sound_card via data port", Model::DataPort, "{\"preset\":\"sound_card\"," + lf + "}", "{" + lf + "}", 8, 1,
       80, 0.9, 1.01},  // the default also works on a fast data port (see README)
  };
  for (const Row& r : rows) {
    std::string name = r.name;
    CAPTURE(name);
    // The receiver uses the same modem as the sender (preset: same preset; default: defaults).
    Result p = run_case(r.preset_cfg, r.preset_cfg, r.model, r.bursts, r.per_burst, r.len, 11);
    Result d = run_case(r.default_cfg, r.default_cfg, r.model, r.bursts, r.per_burst, r.len, 11);
    MESSAGE(name << ": preset " << p.got << "/" << p.sent << " (" << p.key_ups << " key-ups), default " << d.got
                 << "/" << d.sent);
    CHECK(p.rate() >= r.preset_min);
    CHECK(d.rate() < r.default_max);
  }
}

TEST_CASE("presets: handheld_vox holds VOX across back-to-back frames") {
  const std::string cfg = R"({"preset":"handheld_vox","licence":"licence_free"})";
  Result p = run_case(cfg, cfg, Model::Vox, 3, 6, 100, 21);
  MESSAGE("handheld_vox, 6 frames per message: " << p.got << "/" << p.sent << ", key-ups " << p.key_ups);
  CHECK(p.rate() >= 0.95);
  CHECK(p.key_ups == 3);  // one VOX opening per burst: frames are sent back to back, VOX never drops
}

TEST_CASE("presets long: success rates over many seeds" * doctest::skip()) {
  const std::string lf = R"("licence":"licence_free")";
  for (int m = 0; m < 4; ++m) {
    const char* preset = m == 0 ? "handheld_vox" : m == 1 ? "handheld_ptt" : m == 2 ? "acoustic" : "sound_card";
    Model model = m == 0 ? Model::Vox : m == 1 ? Model::Ptt : m == 2 ? Model::Acoustic : Model::DataPort;
    std::string cfg = std::string("{\"preset\":\"") + preset + "\"," + lf + "}";
    Result p = run_case(cfg, cfg, model, m == 2 ? 8 : 20, m == 2 ? 2 : 4, m == 2 ? 40 : 100, 1000);
    Result d = run_case("{" + lf + "}", "{" + lf + "}", model, m == 2 ? 8 : 20, m == 2 ? 2 : 4, m == 2 ? 40 : 100, 1000);
    MESSAGE(preset << ": preset " << p.got << "/" << p.sent << ", default " << d.got << "/" << d.sent);
    CHECK(p.rate() >= 0.9);
  }
}

TEST_CASE("rf.radios database: well-formed, verified entries carry sources, recommendations parse") {
  auto all = rf::radio_profiles();
  REQUIRE(all.size() >= 15);
  std::set<std::string> ids;
  bool uv5r = false;
  for (const auto& r : all) {
    CAPTURE(r.id);
    CHECK(ids.insert(r.id).second);
    CHECK(!r.maker.empty());
    CHECK(!r.model.empty());
    CHECK((r.kind == "handheld" || r.kind == "mobile" || r.kind == "base"));
    CHECK(!r.connector.empty());
    CHECK(!r.settings.empty());
    Json cfg;
    REQUIRE(Json::parse(r.link_config_json, cfg));
    CHECK(cfg.is_object());
    // the recommended link must actually be creatable
    Clock clk;
    FakeHost h(&clk);
    std::string err;
    auto l = create_link(r.link_type, r.link_config_json, h, &err);
    CHECK_MESSAGE(l, err);
    if (r.link_type == "audio") CHECK(cfg.get_string("preset") == r.preset);
    if (!r.verified)
      for (const auto& s : r.settings) CHECK(s.how.find("MENU ") == std::string::npos);  // no menu numbers
    if (r.verified) CHECK(r.notes.find("miklor") != std::string::npos);
    if (r.id == "baofeng_uv5r") uv5r = r.verified;
  }
  CHECK(uv5r);
  CHECK(rf::radio_profiles("uv-5r").size() >= 1);
  CHECK(rf::radio_profiles("zzzz-none").empty());
}

}  // TEST_SUITE
