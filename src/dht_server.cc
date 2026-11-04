#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "fmt/format.h"
#include "gflags/gflags.h"
#include "trpc/common/trpc_app.h"
#include "trpc/log/trpc_log.h"

#include "src/dht_common.h"
#include "src/dht_service.h"

DEFINE_string(node_name, "node-a", "DHT node name, for example node-a");
DEFINE_string(node_id, "0x10000000", "Unsigned 32-bit node id, decimal or 0x-prefixed hex");
DEFINE_string(predecessor_id, "0xB0000000",
              "Unsigned 32-bit predecessor id, decimal or 0x-prefixed hex");
DEFINE_string(successor_service,
              "trpc.dht.node-b.DhtService",
              "tRPC client service name configured for the successor");

namespace lab1 {

namespace {

std::optional<NodeId> ParseNodeId(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }

  std::size_t consumed = 0;
  try {
    const unsigned long long parsed = std::stoull(text, &consumed, 0);
    if (consumed != text.size() ||
        parsed > std::numeric_limits<NodeId>::max()) {
      return std::nullopt;
    }
    return static_cast<NodeId>(parsed);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace

class DhtServer final : public ::trpc::TrpcApp {
 public:
  int Initialize() override {
    const auto node_id = ParseNodeId(FLAGS_node_id);
    const auto predecessor_id = ParseNodeId(FLAGS_predecessor_id);
    if (FLAGS_node_name.empty() || !node_id || !predecessor_id) {
      TRPC_FMT_ERROR(
          "invalid DHT config node_name={} node_id={} predecessor_id={}",
          FLAGS_node_name, FLAGS_node_id, FLAGS_predecessor_id);
      return -1;
    }
    if (FLAGS_successor_service.empty()) {
      TRPC_FMT_ERROR("successor_service must not be empty");
      return -1;
    }

    const auto& config = ::trpc::TrpcConfig::GetInstance()->GetServerConfig();
    const std::string service_name =
        fmt::format("trpc.{}.{}.DhtService", config.app, config.server);
    DhtNodeConfig node_config;
    node_config.node_name = FLAGS_node_name;
    node_config.node_id = *node_id;
    node_config.predecessor_id = *predecessor_id;
    node_config.successor_service_name = FLAGS_successor_service;

    TRPC_FMT_INFO(
        "register service={} node={} node_id={} predecessor_id={} successor_service={}",
        service_name, node_config.node_name, node_config.node_id,
        node_config.predecessor_id, node_config.successor_service_name);
    RegisterService(service_name,
                    std::make_shared<DhtServiceImpl>(std::move(node_config)));
    return 0;
  }

  void Destroy() override {}
};

}  // namespace lab1

int main(int argc, char** argv) {
  lab1::DhtServer server;
  server.Main(argc, argv);
  server.Wait();
  return 0;
}
