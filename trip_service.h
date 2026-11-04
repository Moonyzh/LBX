#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "proto/pricing.trpc.pb.h"
#include "proto/trip.trpc.pb.h"
#include "trpc/client/rpc_service_proxy.h"

namespace lab1 {

// Trip owns an order's lifecycle. Lab 1B replaces the 1A in-memory map with
// the DHT: every order is stored through the node-a entry as
//     key   = "trip:" + trip_id
//     value = Trip.SerializeAsString()
// The RPC contract with ride_client is unchanged.
class TripServiceImpl final : public ::trpc::ride::TripService {
 public:
  TripServiceImpl();

  ::trpc::Status CreateRide(::trpc::ServerContextPtr context,
                            const ::trpc::ride::CreateRideRequest* request,
                            ::trpc::ride::CreateRideReply* reply) override;
  ::trpc::Status GetTrip(::trpc::ServerContextPtr context,
                         const ::trpc::ride::GetTripRequest* request,
                         ::trpc::ride::GetTripReply* reply) override;

 private:
  std::string NextTripId();
  std::shared_ptr<::trpc::RpcServiceProxy> GetPricingProxy();
  std::shared_ptr<::trpc::RpcServiceProxy> GetDhtProxy();

  // trip-service always enters the ring at node-a; it never talks to node-b or
  // node-c directly. The node forwards along its successor when needed.
  static constexpr const char* kDhtEntryService = "trpc.dht.node-a.DhtService";
  static std::string DhtKeyOf(const std::string& trip_id);

  std::shared_ptr<::trpc::RpcServiceProxy> pricing_proxy_;
  std::shared_ptr<::trpc::RpcServiceProxy> dht_proxy_;
  std::mutex proxy_mutex_;

  std::uint64_t next_trip_number_{1};
  std::mutex id_mutex_;
};

}  // namespace lab1
