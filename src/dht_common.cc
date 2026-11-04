#include "src/dht_common.h"

namespace lab1 {

std::uint32_t HashKey(std::string_view text) {
  constexpr std::uint32_t kFnvOffsetBasis = 2166136261U;
  constexpr std::uint32_t kFnvPrime = 16777619U;

  std::uint32_t hash = kFnvOffsetBasis;
  for (const unsigned char byte : text) {
    hash ^= static_cast<std::uint32_t>(byte);
    hash *= kFnvPrime;
  }
  return hash;
}

bool IsOwner(std::uint32_t key_hash, NodeId node_id, NodeId predecessor_id) {
  if (predecessor_id < node_id) {
    return key_hash > predecessor_id && key_hash <= node_id;
  }
  return key_hash > predecessor_id || key_hash <= node_id;
}

}  // namespace lab1
