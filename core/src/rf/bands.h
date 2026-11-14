// EmberMesh core — radio band-plan reference database (static data, mirrors RPC `rf.bands`).
//
// Values are limited to figures we are confident in; rules change and differ per country, so every
// service carries a licence note. This is reference information, not legal advice.
#pragma once

#include <string>
#include <vector>

namespace ember {
namespace rf {

struct BandChannel {
  std::string name;
  double freq_hz;
  double bw_hz;        // 0 = not specified
  std::string notes;   // may be empty
};

struct BandService {
  std::string id;
  std::string name;
  std::string region;     // comma-separated region codes, e.g. "US,CA" or "INTL"
  std::string license;    // licence requirement summary
  double max_power_w;     // 0 = not specified (varies / see notes)
  std::string notes;
  std::vector<BandChannel> channels;
};

// All services, optionally filtered. region: "" = all; otherwise any code (case-insensitive): our
// data-set codes ("US", "EU", "UK", "AU", "NZ", "CA", "JP", "KR", "IN", "AS", "BR", "ZA"), an ISO
// 3166 country code (mapped to the data set that applies, e.g. "DE" -> EU, "SG" -> AS) or anything
// else. A region filter always returns worldwide ("INTL") services and the IARU region ("R1", "R2",
// "R3") services; an unknown code such as "OTHER" gets INTL plus all three IARU regions.
// service: "" = all; otherwise an exact service id.
std::vector<BandService> band_services(const std::string& region = "", const std::string& service = "");

// The region tags a filter code matches (see band_services), most specific first; empty for "".
std::vector<std::string> band_region_tags(const std::string& region);

// Region codes known to the database (for UI selects).
std::vector<std::string> band_regions();

// General notes shown with every band-plan listing (emergency-use note, disclaimer).
std::vector<std::string> band_general_notes();

}  // namespace rf
}  // namespace ember
