#include <cstdint>
#include <iostream>
#include <string_view>

#include "src/dht_common.h"

namespace {

int failures = 0;

void ExpectEqual(std::string_view name, std::uint32_t actual,
                 std::uint32_t expected) {
  if (actual != expected) {
    std::cerr << name << ": expected 0x" << std::hex << expected << ", got 0x"
              << actual << std::dec << '\n';
    ++failures;
  }
}

void ExpectOwner(std::string_view name, std::uint32_t key_hash,
                 lab1::NodeId node_id, lab1::NodeId predecessor_id,
                 bool expected) {
  const bool actual = lab1::IsOwner(key_hash, node_id, predecessor_id);
  if (actual != expected) {
    std::cerr << name << ": expected " << expected << ", got " << actual
              << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using lab1::NodeId;

  constexpr NodeId kNodeA = 0x10000000U;
  constexpr NodeId kNodeB = 0x60000000U;
  constexpr NodeId kNodeC = 0xB0000000U;

  ExpectEqual("FNV-1a(a)", lab1::HashKey("a"), 0xE40C292CU);
  ExpectEqual("FNV-1a(foobar)", lab1::HashKey("foobar"), 0xBF9CF968U);
  ExpectEqual("FNV-1a(key-0408)", lab1::HashKey("key-0408"), 0x007A1D67U);
  ExpectEqual("FNV-1a(key-0020)", lab1::HashKey("key-0020"), 0x51296991U);
  ExpectEqual("FNV-1a(key-0001)", lab1::HashKey("key-0001"), 0xBC2E8F30U);

  ExpectOwner("A owns zero", 0, kNodeA, kNodeC, true);
  ExpectOwner("A owns its id", kNodeA, kNodeA, kNodeC, true);
  ExpectOwner("A does not own predecessor", kNodeC, kNodeA, kNodeC, false);
  ExpectOwner("A owns after predecessor", kNodeC + 1, kNodeA, kNodeC, true);
  ExpectOwner("A owns max id", 0xFFFFFFFFU, kNodeA, kNodeC, true);
  ExpectOwner("A rejects id after self", kNodeA + 1, kNodeA, kNodeC, false);

  ExpectOwner("B rejects predecessor", kNodeA, kNodeB, kNodeA, false);
  ExpectOwner("B owns after predecessor", kNodeA + 1, kNodeB, kNodeA, true);
  ExpectOwner("B owns its id", kNodeB, kNodeB, kNodeA, true);
  ExpectOwner("B rejects id after self", kNodeB + 1, kNodeB, kNodeA, false);

  ExpectOwner("C rejects predecessor", kNodeB, kNodeC, kNodeB, false);
  ExpectOwner("C owns after predecessor", kNodeB + 1, kNodeC, kNodeB, true);
  ExpectOwner("C owns its id", kNodeC, kNodeC, kNodeB, true);
  ExpectOwner("C rejects id after self", kNodeC + 1, kNodeC, kNodeB, false);

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "dht_common_test: all checks passed\n";
  return 0;
}
