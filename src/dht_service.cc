#include "src/dht_service.h"

#include <utility>

#include "trpc/client/make_client_context.h"
#include "trpc/client/trpc_client.h"
#include "trpc/common/status.h"
#include "trpc/log/trpc_log.h"

namespace lab1 {

namespace {

constexpr std::uint32_t kMaxHops = 3;

}  // namespace

DhtServiceImpl::DhtServiceImpl(DhtNodeConfig config) : config_(std::move(config)) {}

std::shared_ptr<::trpc::RpcServiceProxy> DhtServiceImpl::GetSuccessorProxy() {
  if (config_.successor_service_name.empty()) {
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(proxy_mutex_);
  if (!successor_proxy_) {
    successor_proxy_ = ::trpc::GetTrpcClient()->GetProxy<::trpc::RpcServiceProxy>(
        config_.successor_service_name);
  }
  return successor_proxy_;
}

::trpc::Status DhtServiceImpl::Put(::trpc::ServerContextPtr context,
                                   const ::trpc::dht::PutRequest* request,
                                   ::trpc::dht::PutReply* reply) {
  const std::uint32_t key_hash = HashKey(request->key());
  const std::uint32_t hops = request->hops();

  if (request->key().empty()) {
    reply->set_ok(false);
    reply->set_hops(hops);
    reply->set_message("key is required");
    TRPC_FMT_WARN("node={} key_hash={} action=put result=rejected_empty_key hops={}",
                  config_.node_name, key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  if (hops >= kMaxHops) {
    reply->set_ok(false);
    reply->set_hops(hops);
    reply->set_message("hop limit reached before reaching owner");
    TRPC_FMT_WARN(
        "node={} key={} key_hash={} action=put result=hop_limit hops={}",
        config_.node_name, request->key(), key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  if (IsOwner(key_hash, config_.node_id, config_.predecessor_id)) {
    {
      std::lock_guard<std::mutex> lock(values_mutex_);
      values_[request->key()] = request->value();
    }
    reply->set_ok(true);
    reply->set_owner(config_.node_name);
    reply->set_hops(hops);
    reply->set_message("value stored on owner");
    TRPC_FMT_INFO(
        "node={} key={} key_hash={} owner={} action=put result=stored hops={}",
        config_.node_name, request->key(), key_hash, config_.node_name, hops);
    return ::trpc::kSuccStatus;
  }

  const auto successor_proxy = GetSuccessorProxy();
  if (!successor_proxy) {
    reply->set_ok(false);
    reply->set_hops(hops);
    reply->set_message("successor proxy is unavailable");
    TRPC_FMT_ERROR(
        "node={} key={} key_hash={} action=put result=missing_successor hops={}",
        config_.node_name, request->key(), key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  ::trpc::dht::PutRequest forwarded_request = *request;
  forwarded_request.set_hops(hops + 1);
  ::trpc::dht::PutReply forwarded_reply;
  const auto client_context = ::trpc::MakeClientContext(context, successor_proxy);
  client_context->SetFuncName("/trpc.dht.DhtService/Put");
  const auto status =
      successor_proxy->UnaryInvoke<::trpc::dht::PutRequest, ::trpc::dht::PutReply>(
          client_context, forwarded_request, &forwarded_reply);
  if (!status.OK()) {
    reply->set_ok(false);
    reply->set_hops(forwarded_request.hops());
    reply->set_message("successor Put RPC failed: " + status.ErrorMessage());
    TRPC_FMT_ERROR(
        "node={} key={} key_hash={} action=forward_put result=rpc_error hops={} error={}",
        config_.node_name, request->key(), key_hash, forwarded_request.hops(),
        status.ErrorMessage());
    return ::trpc::kSuccStatus;
  }

  *reply = forwarded_reply;
  TRPC_FMT_INFO(
      "node={} key={} key_hash={} action=forward_put owner={} result={} hops={}",
      config_.node_name, request->key(), key_hash, reply->owner(), reply->ok(),
      reply->hops());
  return ::trpc::kSuccStatus;
}

::trpc::Status DhtServiceImpl::Get(::trpc::ServerContextPtr context,
                                   const ::trpc::dht::GetRequest* request,
                                   ::trpc::dht::GetReply* reply) {
  const std::uint32_t key_hash = HashKey(request->key());
  const std::uint32_t hops = request->hops();

  if (request->key().empty()) {
    reply->set_found(false);
    reply->set_hops(hops);
    reply->set_message("key is required");
    TRPC_FMT_WARN("node={} key_hash={} action=get result=rejected_empty_key hops={}",
                  config_.node_name, key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  if (hops >= kMaxHops) {
    reply->set_found(false);
    reply->set_hops(hops);
    reply->set_message("hop limit reached before reaching owner");
    TRPC_FMT_WARN(
        "node={} key={} key_hash={} action=get result=hop_limit hops={}",
        config_.node_name, request->key(), key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  if (IsOwner(key_hash, config_.node_id, config_.predecessor_id)) {
    std::string value;
    bool found = false;
    {
      std::lock_guard<std::mutex> lock(values_mutex_);
      const auto it = values_.find(request->key());
      if (it != values_.end()) {
        value = it->second;
        found = true;
      }
    }
    reply->set_found(found);
    reply->set_owner(config_.node_name);
    reply->set_hops(hops);
    *reply->mutable_value() = std::move(value);
    reply->set_message(found ? "value read from owner" : "key not found on owner");
    TRPC_FMT_INFO(
        "node={} key={} key_hash={} owner={} action=get result={} hops={}",
        config_.node_name, request->key(), key_hash, config_.node_name, found, hops);
    return ::trpc::kSuccStatus;
  }

  const auto successor_proxy = GetSuccessorProxy();
  if (!successor_proxy) {
    reply->set_found(false);
    reply->set_hops(hops);
    reply->set_message("successor proxy is unavailable");
    TRPC_FMT_ERROR(
        "node={} key={} key_hash={} action=get result=missing_successor hops={}",
        config_.node_name, request->key(), key_hash, hops);
    return ::trpc::kSuccStatus;
  }

  ::trpc::dht::GetRequest forwarded_request = *request;
  forwarded_request.set_hops(hops + 1);
  ::trpc::dht::GetReply forwarded_reply;
  const auto client_context = ::trpc::MakeClientContext(context, successor_proxy);
  client_context->SetFuncName("/trpc.dht.DhtService/Get");
  const auto status =
      successor_proxy->UnaryInvoke<::trpc::dht::GetRequest, ::trpc::dht::GetReply>(
          client_context, forwarded_request, &forwarded_reply);
  if (!status.OK()) {
    reply->set_found(false);
    reply->set_hops(forwarded_request.hops());
    reply->set_message("successor Get RPC failed: " + status.ErrorMessage());
    TRPC_FMT_ERROR(
        "node={} key={} key_hash={} action=forward_get result=rpc_error hops={} error={}",
        config_.node_name, request->key(), key_hash, forwarded_request.hops(),
        status.ErrorMessage());
    return ::trpc::kSuccStatus;
  }

  *reply = forwarded_reply;
  TRPC_FMT_INFO(
      "node={} key={} key_hash={} action=forward_get owner={} result={} hops={}",
      config_.node_name, request->key(), key_hash, reply->owner(), reply->found(),
      reply->hops());
  return ::trpc::kSuccStatus;
}

}  // namespace lab1
