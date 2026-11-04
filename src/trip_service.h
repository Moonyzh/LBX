#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "proto/pricing.trpc.pb.h"
#include "proto/trip.trpc.pb.h"
#include "trpc/client/rpc_service_proxy.h"

namespace lab1 {

// Trip owns an order's lifecycle. The in-memory map is intentional in 1A:
// it keeps the exercise on service-to-service RPC. Lab 1B replaces this map
// with a DHT-backed repository without changing CreateRide or GetTrip.
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

  std::shared_ptr<::trpc::RpcServiceProxy> pricing_proxy_;
  std::mutex proxy_mutex_;
  std::unordered_map<std::string, ::trpc::ride::Trip> trips_;
  std::mutex trips_mutex_;
  std::uint64_t next_trip_number_{1};
};

}  // namespace lab1
