// EmberMesh core / link — registry entries of every driver (one per *_link.cpp).
#pragma once

#include "link/link_util.h"

namespace ember {
namespace link {

const LinkTypeInfo& raw_type();
const LinkTypeInfo& stream_type();
const LinkTypeInfo& kiss_type();
const LinkTypeInfo& audio_type();
const LinkTypeInfo& meshtastic_type();
const LinkTypeInfo& rnode_type();
const LinkTypeInfo& lora_at_type();
const LinkTypeInfo& rtl_tcp_type();

// All registered types in catalogue order.
const std::vector<const LinkTypeInfo*>& all_types();
const LinkTypeInfo* find_type(const std::string& type);

}  // namespace link
}  // namespace ember
