#pragma once

#include <cstdint>
#include <string_view>

namespace lab1 {

using NodeId = std::uint32_t;

std::uint32_t HashKey(std::string_view text);

// Returns true when key_hash belongs to the interval
// (predecessor_id, node_id], including the wrap-around interval.
bool IsOwner(std::uint32_t key_hash, NodeId node_id, NodeId predecessor_id);

}  // namespace lab1
