#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "proto/dht.trpc.pb.h"
#include "src/dht_common.h"
#include "trpc/client/rpc_service_proxy.h"

namespace lab1 {

struct DhtNodeConfig {
  std::string node_name;
  NodeId node_id{0};
  NodeId predecessor_id{0};
  std::string successor_service_name;
};

class DhtServiceImpl final : public ::trpc::dht::DhtService {
 public:
  explicit DhtServiceImpl(DhtNodeConfig config);

  ::trpc::Status Put(::trpc::ServerContextPtr context,
                     const ::trpc::dht::PutRequest* request,
                     ::trpc::dht::PutReply* reply) override;
  ::trpc::Status Get(::trpc::ServerContextPtr context,
                     const ::trpc::dht::GetRequest* request,
                     ::trpc::dht::GetReply* reply) override;

 private:
  std::shared_ptr<::trpc::RpcServiceProxy> GetSuccessorProxy();

  DhtNodeConfig config_;
  std::unordered_map<std::string, std::string> values_;
  std::mutex values_mutex_;
  std::shared_ptr<::trpc::RpcServiceProxy> successor_proxy_;
  std::mutex proxy_mutex_;
};

}  // namespace lab1
