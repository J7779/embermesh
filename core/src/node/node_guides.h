// EmberMesh node — JSON bindings of the guide library's semantic methods (docs/RPC_API.md §7):
// guides.search (with `semantic`, `match`, `passage`), guides.related, guides.ask, guides.info.
//
// Free functions over a GuideLibrary so they can be unit-tested without a Node. Node's method
// table dispatches to them (node_lib.cpp m_guides_search; guides.related / guides.ask /
// guides.info are registered with one-line Node members that forward here).
#pragma once

#include "guides/library.h"
#include "node/rpc_util.h"

namespace ember::node {

RpcResult guides_search_rpc(const guides::GuideLibrary& lib, Params& p);
RpcResult guides_related_rpc(const guides::GuideLibrary& lib, Params& p);
RpcResult guides_ask_rpc(const guides::GuideLibrary& lib, Params& p);
RpcResult guides_info_rpc(const guides::GuideLibrary& lib, Params& p);

// GuideMeta as the JSON object documented for guides.list / guides.get.
Json guide_meta_to_json(const guides::GuideMeta& m);

}  // namespace ember::node
