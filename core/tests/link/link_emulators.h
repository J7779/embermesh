// Honest device emulators for the handshake drivers (meshtastic, rnode, lora_at, rtl_tcp), used by
// the link mutation fuzzer to check that a driver is never permanently wedged: after any amount of
// hostile input, a clean device session must still bring the link Up. Deliberately minimal (the
// per-driver tests have richer, scripted emulators); never throws on odd input.
#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include "ember/bytes.h"
#include "framing/slip.h"
#include "link/meshtastic_proto.h"

namespace linkemu {

using ember::Bytes;
using ember::ByteView;

struct Emulator {
  virtual ~Emulator() = default;
  virtual void from_host(ByteView v) = 0;  // bytes the driver wrote
  Bytes outbox;                            // bytes the device sends back
  bool honest = true;                      // answers at all
  Bytes take() { Bytes b; b.swap(outbox); return b; }
};

// Meshtastic serial client API: answers want_config with my_info, node_info, config, metadata and
// config_complete(nonce).
struct MeshtasticEmu : Emulator {
  explicit MeshtasticEmu(bool ble) : ble_(ble) {}
  void from_host(ByteView v) override {
    if (ble_) { handle(v); return; }
    dfr_.feed(v, [&](ByteView pb) { handle(pb); }, [](char) {});
  }
  void say(const ember::link::mt::FromRadio& f) {
    Bytes pb = ember::link::mt::encode_from_radio(f);
    Bytes w = ble_ ? pb : ember::link::mt::frame(ByteView(pb));
    if (ble_) datagrams.push_back(w);
    else outbox.insert(outbox.end(), w.begin(), w.end());
  }
  std::vector<Bytes> datagrams;  // BLE framing: one protobuf per endpoint datagram

 private:
  void handle(ByteView pb) {
    namespace mt = ember::link::mt;
    mt::ToRadio t;
    if (!honest || !mt::decode_to_radio(pb, &t)) return;
    if (t.kind != mt::ToKind::WantConfig) return;
    mt::FromRadio f;
    f.kind = mt::FromKind::MyInfo; f.my_node_num = 0x11223344; say(f);
    f = mt::FromRadio(); f.kind = mt::FromKind::NodeInfo; f.node.num = 0x11223344; f.node.long_name = "Emu"; say(f);
    f = mt::FromRadio(); f.kind = mt::FromKind::Config; f.has_lora = true; f.lora.use_preset = true; f.lora.region = 1; say(f);
    f = mt::FromRadio(); f.kind = mt::FromKind::Metadata; f.firmware_version = "2.5.6"; say(f);
    f = mt::FromRadio(); f.kind = mt::FromKind::ConfigComplete; f.config_complete_id = t.want_config_id; say(f);
  }
  bool ble_;
  ember::link::mt::Deframer dfr_;
};

// RNode firmware: detect / version / platform / MCU, echoes configuration, READY after DATA.
struct RNodeEmu : Emulator {
  void from_host(ByteView v) override { dec_.feed(v, [&](ByteView f) { handle(f); }); }

 private:
  void reply(const Bytes& body) {
    outbox.push_back(0xC0);
    ember::slip::escape_append(ByteView(body), outbox);
    outbox.push_back(0xC0);
  }
  void handle(ByteView f) {
    if (!honest || f.empty()) return;
    uint8_t c = f[0];
    ByteView a = f.sub(1);
    switch (c) {
      case 0x08: if (a.size() == 1 && a[0] == 0x73) reply({0x08, 0x46}); break;
      case 0x50: reply({0x50, 0x01, 0x4A}); break;
      case 0x48: reply({0x48, 0x80}); break;
      case 0x49: reply({0x49, 0x81}); break;
      case 0x01: case 0x02: if (a.size() == 4) reply(f.to_bytes()); break;
      case 0x03: case 0x04: case 0x05: case 0x06: if (a.size() == 1) reply(f.to_bytes()); break;
      case 0x0B: case 0x0C: if (a.size() == 2) reply(f.to_bytes()); break;
      case 0x00: reply({0x0F}); break;
      default: break;
    }
  }
  ember::slip::Decoder dec_{1024, true};
};

// Reyax RYLR AT command module.
struct AtEmu : Emulator {
  void from_host(ByteView v) override {
    for (uint8_t b : v) {
      if (b == '\n') {
        if (!line_.empty() && line_.back() == '\r') line_.pop_back();
        handle(line_);
        line_.clear();
      } else if (line_.size() < 1000) {
        line_.push_back(static_cast<char>(b));
      }
    }
  }

 private:
  void say(const std::string& s) { outbox.insert(outbox.end(), s.begin(), s.end()); outbox.push_back('\r'); outbox.push_back('\n'); }
  void handle(const std::string& c) {
    if (!honest) return;
    auto starts = [&](const char* p) { return c.rfind(p, 0) == 0; };
    if (c == "AT") say("+OK");
    else if (c == "AT+VER?") say("+VER=RYLR998_V1.2.3");
    else if (c == "AT+ADDRESS?") say("+ADDRESS=" + std::to_string(addr_));
    else if (starts("AT+ADDRESS=")) { addr_ = std::atoi(c.c_str() + 11); say("+OK"); }
    else if (c == "AT+NETWORKID?") say("+NETWORKID=" + std::to_string(net_));
    else if (starts("AT+NETWORKID=")) { net_ = std::atoi(c.c_str() + 13); say("+OK"); }
    else if (c == "AT+BAND?") say("+BAND=" + band_);
    else if (starts("AT+BAND=")) { band_ = c.substr(8); say("+OK"); }
    else if (c == "AT+PARAMETER?") say("+PARAMETER=" + param_);
    else if (starts("AT+PARAMETER=")) { param_ = c.substr(13); say("+OK"); }
    else if (starts("AT+CRFOP=") || starts("AT+CPIN=") || starts("AT+SEND=")) say("+OK");
    else say("+ERR=2");
  }
  std::string line_, band_ = "915000000", param_ = "9,7,1,12";
  int addr_ = 0, net_ = 18;
};

// rtl_tcp server: header once per connection (the fuzzer calls reset() on reconnect), then IQ.
struct RtlEmu : Emulator {
  void from_host(ByteView) override {
    if (!honest || sent_) return;
    const uint8_t hdr[12] = {'R', 'T', 'L', '0', 0, 0, 0, 5, 0, 0, 0, 29};
    outbox.insert(outbox.end(), hdr, hdr + 12);
    for (int i = 0; i < 4000; ++i) outbox.push_back(static_cast<uint8_t>(127 + (i % 7) - 3));
    sent_ = true;
  }
  void reset() { sent_ = false; }

 private:
  bool sent_ = false;
};

}  // namespace linkemu
