// SPDX-License-Identifier: MIT
// Builds without EMBER_HOST_MDNS (phones do their own discovery): Nearby and MdnsAgent exist as
// inert types so the Host compiles unchanged; Host never creates a Nearby in such builds.
#ifndef EMBER_HOST_HAVE_MDNS
#include "host/nearby.h"

namespace ember_host {

MdnsAgent::MdnsAgent(Loop& loop) : loop_(loop) {}
MdnsAgent::~MdnsAgent() = default;
void MdnsAgent::stop() {}

Nearby::Nearby(Loop& loop, EndpointSink& sink, NearbyHostApi& host, std::string service)
    : loop_(loop), sink_(sink), host_(host), service_(std::move(service)), mdns_(loop) {}
Nearby::~Nearby() = default;
void Nearby::set_enabled(bool, const std::string&) {}
void Nearby::link_gone(uint32_t) {}
Json Nearby::status() const {
  Json s = Json::object();
  s.set("enabled", false);
  s.set("running", false);
  s.set("error", "this build has no Nearby support (EMBER_HOST_MDNS=OFF)");
  return s;
}

}  // namespace ember_host
#endif
