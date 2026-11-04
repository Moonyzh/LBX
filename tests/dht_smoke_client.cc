#include <cstdint>
#include <iostream>
#include <string>

#include "gflags/gflags.h"
#include "trpc/client/make_client_context.h"
#include "trpc/client/rpc_service_proxy.h"
#include "trpc/client/trpc_client.h"
#include "trpc/common/runtime_manager.h"

#include "proto/dht.trpc.pb.h"

DEFINE_string(client_config, "config/dht-smoke-client.local.yaml",
              "tRPC client yaml path");
DEFINE_string(entry, "trpc.dht.node-a.DhtService",
              "Entry DHT service name");
DEFINE_string(op, "get", "Operation: put or get");
DEFINE_string(key, "key-0408", "DHT key");
DEFINE_string(value, "smoke-value", "Value used by put");
DEFINE_uint32(hops, 0, "Initial hop count");

namespace {

std::string ToHex(const std::string& value) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() * 2);
  for (const unsigned char byte : value) {
    result.push_back(kHex[byte >> 4]);
    result.push_back(kHex[byte & 0x0F]);
  }
  return result;
}

int Run() {
  const auto proxy = ::trpc::GetTrpcClient()->GetProxy<::trpc::RpcServiceProxy>(
      FLAGS_entry);
  if (!proxy) {
    std::cerr << "cannot create proxy for entry=" << FLAGS_entry << '\n';
    return 2;
  }
  const auto context = ::trpc::MakeClientContext(proxy);

  if (FLAGS_op == "put") {
    ::trpc::dht::PutRequest request;
    request.set_key(FLAGS_key);
    request.set_value(FLAGS_value);
    request.set_hops(FLAGS_hops);
    ::trpc::dht::PutReply reply;
    context->SetFuncName("/trpc.dht.DhtService/Put");
    const auto status =
        proxy->UnaryInvoke<::trpc::dht::PutRequest, ::trpc::dht::PutReply>(
            context, request, &reply);
    if (!status.OK()) {
      std::cerr << "put_rpc_error=" << status.ErrorMessage() << '\n';
      return 2;
    }
    std::cout << "operation=put ok=" << reply.ok()
              << " owner=" << reply.owner() << " hops=" << reply.hops()
              << " message=" << reply.message() << '\n';
    return reply.ok() ? 0 : 1;
  }

  if (FLAGS_op == "get") {
    ::trpc::dht::GetRequest request;
    request.set_key(FLAGS_key);
    request.set_hops(FLAGS_hops);
    ::trpc::dht::GetReply reply;
    context->SetFuncName("/trpc.dht.DhtService/Get");
    const auto status =
        proxy->UnaryInvoke<::trpc::dht::GetRequest, ::trpc::dht::GetReply>(
            context, request, &reply);
    if (!status.OK()) {
      std::cerr << "get_rpc_error=" << status.ErrorMessage() << '\n';
      return 2;
    }
    std::cout << "operation=get found=" << reply.found()
              << " owner=" << reply.owner() << " hops=" << reply.hops()
              << " value_hex=" << ToHex(reply.value())
              << " message=" << reply.message() << '\n';
    return reply.found() ? 0 : 1;
  }

  std::cerr << "--op must be put or get\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (::trpc::TrpcConfig::GetInstance()->Init(FLAGS_client_config) != 0) {
    std::cerr << "cannot load client config: " << FLAGS_client_config << '\n';
    return 2;
  }
  return ::trpc::RunInTrpcRuntime([] { return Run(); });
}
