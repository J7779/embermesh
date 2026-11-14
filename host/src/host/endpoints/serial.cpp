// "serial" endpoint: USB/RS-232 TNCs, Meshtastic/RNode over USB, LoRa AT modules, radios with a
// built-in TNC. Bytes only; KISS/Meshtastic/... framing lives in core link drivers.
#include <memory>

#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/endpoints/serial_port.h"
#include "host/util.h"

namespace ember_host {

namespace {

class SerialEndpoint : public Endpoint {
 public:
  SerialEndpoint(Loop& loop, EndpointSink& sink, const Json& config, const SerialConfig& sc, std::string ptt_line,
                 bool ptt_invert)
      : Endpoint(loop, sink, config), sc_(sc), ptt_line_(std::move(ptt_line)), ptt_invert_(ptt_invert) {}
  ~SerialEndpoint() override { close(); }

  const char* kind() const override { return "serial"; }

  void open() override {
    close();
    token_ = std::make_shared<bool>(true);
    std::weak_ptr<bool> tok = token_;
    port_ = std::make_unique<SerialPort>(&loop());
    port_->on_data = [this](const uint8_t* d, size_t n) { deliver(d, n); };
    port_->on_error = [this](const std::string& why) {
      EH_LOGW("serial %s: %s", sc_.port.c_str(), why.c_str());
      ptt_on_ = false;
      set_state(false, why);
    };
    std::string err;
    bool ok = port_->open(sc_, false, &err);
    if (ok && ptt_line_ != "none") apply_ptt(false);
    loop().timer(0, [this, tok, ok, err] {
      if (tok.expired()) return;
      if (ok) {
        EH_LOGI("serial %s open at %u baud", sc_.port.c_str(), unsigned(sc_.baud));
        set_state(true);
      } else {
        port_.reset();
        set_state(false, err);
      }
    });
  }

  void close() override {
    token_.reset();
    if (port_) {
      if (ptt_on_ && port_->is_open()) apply_ptt(false);
      port_->close();
      // Destroy later: close() may be called from inside the port's own callback.
      SerialPort* raw = port_.release();
      loop().timer(0, [raw] { delete raw; });
    }
    ptt_on_ = false;
    connected_ = false;
  }

  void write(const uint8_t* data, size_t len) override {
    if (port_ && port_->is_open()) port_->write(data, len);
  }

  void set_ptt(bool on) override {
    if (ptt_line_ == "none") return;
    if (!port_ || !port_->is_open()) return;
    apply_ptt(on);
  }

  Json status() const override {
    Json s = Json::object();
    s.set("port", sc_.port);
    s.set("baud", sc_.baud);
    s.set("ptt", ptt_on_);
    s.set("ptt_line", ptt_line_);
    if (port_) {
      s.set("tx_pending", port_->pending());
      if (port_->dropped_bytes()) s.set("tx_dropped", Json(port_->dropped_bytes()));
    }
    return s;
  }

 private:
  void apply_ptt(bool on) {
    ptt_on_ = on;
    bool level = on != ptt_invert_;
    std::string err;
    bool ok = ptt_line_ == "rts" ? port_->set_rts(level, &err) : port_->set_dtr(level, &err);
    if (!ok) EH_LOGW("serial %s: PTT %s: %s", sc_.port.c_str(), ptt_line_.c_str(), err.c_str());
  }

  SerialConfig sc_;
  std::string ptt_line_;
  bool ptt_invert_;
  bool ptt_on_ = false;
  std::unique_ptr<SerialPort> port_;
  std::shared_ptr<bool> token_;
};

Json serial_fields() {
  Json f = Json::array();
  f.push(Field("port", "Serial port", "text")
             .required()
             .help("Device path: /dev/cu.usbserial-XXXX (macOS), /dev/ttyUSB0 or /dev/ttyACM0 (Linux), COM3 (Windows)"));
  f.push(Field("baud", "Baud rate", "number")
             .def(9600)
             .min(50)
             .max(12000000)
             .unit("bd")
             .help("Common: 1200, 4800, 9600 (most KISS TNCs), 19200, 38400, 57600, 115200 (Meshtastic, RNode)"));
  f.push(Field("data_bits", "Data bits", "select").def(8).option(8, "8").option(7, "7").option(6, "6").option(5, "5").advanced());
  f.push(Field("parity", "Parity", "select")
             .def("none")
             .option("none", "None")
             .option("even", "Even")
             .option("odd", "Odd")
             .option("mark", "Mark")
             .option("space", "Space")
             .advanced());
  f.push(Field("stop_bits", "Stop bits", "select").def(1).option(1, "1").option(2, "2").advanced());
  f.push(Field("flow", "Flow control", "select")
             .def("none")
             .option("none", "None")
             .option("rtscts", "Hardware (RTS/CTS)")
             .option("xonxoff", "Software (XON/XOFF)")
             .advanced());
  f.push(Field("dtr", "DTR line", "select")
             .def("on")
             .option("on", "On (asserted)")
             .option("off", "Off")
             .help("Some devices (e.g. ESP32 boards) reset when DTR/RTS toggle")
             .advanced());
  f.push(Field("rts", "RTS line", "select").def("on").option("on", "On (asserted)").option("off", "Off").advanced());
  f.push(Field("ptt_line", "PTT keying line", "select")
             .def("none")
             .option("none", "None (TNC / VOX keys the radio)")
             .option("rts", "RTS")
             .option("dtr", "DTR")
             .help("Key the transmitter on this line when the link asks for PTT"));
  f.push(Field("ptt_invert", "Invert PTT line", "bool").def(false).advanced());
  f.push(Field("exclusive", "Exclusive access", "bool").def(true).advanced());
  return f;
}

std::unique_ptr<Endpoint> create_serial(Loop& loop, EndpointSink& sink, const Json& config, std::string* error) {
  SerialConfig sc;
  if (!serial_config_from_json(config, &sc, error)) return nullptr;
  std::string ptt = to_lower(trim(cfg_str(config, "ptt_line", "none")));
  if (ptt.empty()) ptt = "none";
  if (ptt != "none" && ptt != "rts" && ptt != "dtr") {
    if (error) *error = "invalid ptt_line (none/rts/dtr): " + ptt;
    return nullptr;
  }
  if (ptt == "rts" && sc.flow == "rtscts") {
    if (error) *error = "cannot key PTT on RTS while RTS/CTS flow control is enabled";
    return nullptr;
  }
  return std::make_unique<SerialEndpoint>(loop, sink, config, sc, ptt, cfg_bool(config, "ptt_invert", false));
}

}  // namespace

Json scan_serial(const Json& /*params*/) {
  Json devices = Json::array();
  for (const auto& d : enumerate_serial_ports()) {
    Json j = Json::object();
    j.set("id", d.path);
    j.set("name", d.name);
    if (!d.detail.empty()) j.set("detail", d.detail);
    Json c = Json::object();
    c.set("port", d.path);
    c.set("baud", 9600);
    j.set("config", c);
    // Radio audio interfaces that key PTT with this port's RTS/DTR line (the "Connect a radio"
    // wizard pre-fills the audio endpoint's ptt / ptt_port from this).
    std::string hay = to_lower(d.name + " " + d.detail + " " + d.path);
    const char* iface = nullptr;
    if (hay.find("aioc") != std::string::npos || hay.find("all-in-one") != std::string::npos || hay.find("1209:7388") != std::string::npos)
      iface = "AIOC (All-In-One-Cable): PTT on this port (serial_dtr; or use cm108 HID mode)";
    else if (hay.find("digirig") != std::string::npos || hay.find("cp210") != std::string::npos || hay.find("slab_usbtouart") != std::string::npos ||
             hay.find("10c4:ea60") != std::string::npos)
      iface = "Digirig / CP210x: PTT on this port's RTS line (serial_rts)";
    else if (hay.find("ftdi") != std::string::npos || hay.find("usbserial") != std::string::npos || hay.find("ch34") != std::string::npos ||
             hay.find("pl2303") != std::string::npos)
      iface = "USB-serial adapter: can key PTT with RTS or DTR through a transistor/opto interface";
    if (iface) {
      j.set("ptt_hint", iface);
      Json pc = Json::object();
      pc.set("ptt", std::string(iface).find("serial_dtr") != std::string::npos ? "serial_dtr" : "serial_rts");
      pc.set("ptt_port", d.path);
      j.set("audio_ptt_config", pc);
    }
    devices.push(j);
  }
  Json r = Json::object();
  r.set("devices", devices);
  return r;
}

namespace {
}  // namespace

void register_serial_endpoints(EndpointRegistry& r) {
  EndpointKind k;
  k.kind = "serial";
  k.title = "Serial port / USB TNC";
  k.description =
      "USB or RS-232 serial device: hardware KISS TNC, radio with built-in TNC, Meshtastic or RNode over USB, "
      "LoRa AT-command module. Optional RTS/DTR PTT keying.";
  k.scannable = true;
  k.config = serial_fields();
  k.link_types = {"kiss", "stream", "meshtastic", "rnode", "lora_at", "raw"};
  k.default_link_type = "kiss";
  k.create = create_serial;
  k.scan = scan_serial;
  r.add(std::move(k));
}

}  // namespace ember_host
