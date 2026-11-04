#include "src/trip_service.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "proto/dht.trpc.pb.h"
#include "trpc/client/make_client_context.h"
#include "trpc/client/trpc_client.h"
#include "trpc/common/status.h"
#include "trpc/log/trpc_log.h"

namespace lab1 {

namespace {

bool IsBlank(const std::string& value) {
  return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

TripServiceImpl::TripServiceImpl() = default;

std::string TripServiceImpl::DhtKeyOf(const std::string& trip_id) {
  return "trip:" + trip_id;
}

std::string TripServiceImpl::NextTripId() {
  std::lock_guard<std::mutex> lock(id_mutex_);
  std::ostringstream stream;
  stream << "trip-" << std::setfill('0') << std::setw(6) << next_trip_number_++;
  return stream.str();
}

std::shared_ptr<::trpc::RpcServiceProxy> TripServiceImpl::GetPricingProxy() {
  std::lock_guard<std::mutex> lock(proxy_mutex_);
  if (!pricing_proxy_) {
    pricing_proxy_ = ::trpc::GetTrpcClient()->GetProxy<::trpc::RpcServiceProxy>(
        "trpc.ride.pricing.PricingService");
  }
  return pricing_proxy_;
}

std::shared_ptr<::trpc::RpcServiceProxy> TripServiceImpl::GetDhtProxy() {
  std::lock_guard<std::mutex> lock(proxy_mutex_);
  if (!dht_proxy_) {
    // The client service with this name is declared in config/trip-service.yaml
    // and points at node-a only. Ring traversal happens inside the DHT nodes.
    dht_proxy_ =
        ::trpc::GetTrpcClient()->GetProxy<::trpc::RpcServiceProxy>(kDhtEntryService);
  }
  return dht_proxy_;
}

::trpc::Status TripServiceImpl::CreateRide(
    ::trpc::ServerContextPtr context, const ::trpc::ride::CreateRideRequest* request,
    ::trpc::ride::CreateRideReply* reply) {
  if (IsBlank(request->passenger_id()) || IsBlank(request->city()) || IsBlank(request->origin()) ||
      IsBlank(request->destination()) || !std::isfinite(request->distance_km()) ||
      request->distance_km() <= 0.0) {
    reply->set_ok(false);
    reply->set_message(
        "passenger, city, origin, destination and a finite positive distance are required");
    return ::trpc::kSuccStatus;
  }

  // Ask the policy owner for a quote; do not duplicate the fare formula here.
  const auto pricing_proxy = GetPricingProxy();
  if (!pricing_proxy) {
    reply->set_ok(false);
    reply->set_message("pricing-service proxy is unavailable");
    return ::trpc::kSuccStatus;
  }

  ::trpc::ride::EstimateFareRequest pricing_request;
  pricing_request.set_city(request->city());
  pricing_request.set_distance_km(request->distance_km());
  pricing_request.set_time_slot(request->time_slot());
  ::trpc::ride::EstimateFareReply pricing_reply;
  const auto pricing_context = ::trpc::MakeClientContext(context, pricing_proxy);
  pricing_context->SetFuncName("/trpc.ride.PricingService/EstimateFare");
  const ::trpc::Status pricing_status =
      pricing_proxy->UnaryInvoke<::trpc::ride::EstimateFareRequest,
                                 ::trpc::ride::EstimateFareReply>(
          pricing_context, pricing_request, &pricing_reply);
  if (!pricing_status.OK() || !pricing_reply.ok()) {
    reply->set_ok(false);
    reply->set_message(pricing_status.OK() ? pricing_reply.message()
                                           : "pricing-service RPC failed: " + pricing_status.ErrorMessage());
    TRPC_FMT_WARN("create ride passenger_id={} downstream=pricing-service result={}",
                  request->passenger_id(), reply->message());
    return ::trpc::kSuccStatus;
  }

  ::trpc::ride::Trip trip;
  const std::string trip_id = NextTripId();
  trip.set_trip_id(trip_id);
  trip.set_passenger_id(request->passenger_id());
  trip.set_city(request->city());
  trip.set_origin(request->origin());
  trip.set_destination(request->destination());
  trip.set_distance_km(request->distance_km());
  trip.set_time_slot(request->time_slot());
  trip.set_estimated_fare(pricing_reply.total_fare());
  trip.set_status("WAITING");

  // Lab 1B: Trip.SerializeAsString() -> Put("trip:" + trip_id, bytes).
  const std::string dht_key = DhtKeyOf(trip_id);
  const std::string dht_value = trip.SerializeAsString();
  const auto dht_proxy = GetDhtProxy();
  if (!dht_proxy) {
    reply->set_ok(false);
    reply->set_message("dht entry node-a proxy is unavailable");
    return ::trpc::kSuccStatus;
  }

  ::trpc::dht::PutRequest put_request;
  put_request.set_key(dht_key);
  put_request.set_value(dht_value);
  put_request.set_hops(0);
  ::trpc::dht::PutReply put_reply;
  const auto dht_context = ::trpc::MakeClientContext(context, dht_proxy);
  dht_context->SetFuncName("/trpc.dht.DhtService/Put");
  const ::trpc::Status put_status =
      dht_proxy->UnaryInvoke<::trpc::dht::PutRequest, ::trpc::dht::PutReply>(
          dht_context, put_request, &put_reply);
  if (!put_status.OK() || !put_reply.ok()) {
    // Do not hand back a trip id that the ring never accepted.
    reply->set_ok(false);
    reply->set_message(put_status.OK() ? "dht put failed: " + put_reply.message()
                                       : "dht put rpc failed: " + put_status.ErrorMessage());
    TRPC_FMT_WARN("create ride trip_id={} dht_key={} put=0 passenger_id={} result={}", trip_id,
                  dht_key, request->passenger_id(), reply->message());
    return ::trpc::kSuccStatus;
  }

  *reply->mutable_trip() = trip;
  reply->set_ok(true);
  reply->set_message("trip created after pricing quote and stored in dht node-a entry");
  TRPC_FMT_INFO(
      "trip_id={} passenger_id={} downstream=pricing-service estimated_fare={} dht_key={} "
      "dht_owner={} dht_hops={} bytes={}",
      trip.trip_id(), trip.passenger_id(), trip.estimated_fare(), dht_key, put_reply.owner(),
      put_reply.hops(), dht_value.size());
  return ::trpc::kSuccStatus;
}

::trpc::Status TripServiceImpl::GetTrip(::trpc::ServerContextPtr context,
                                         const ::trpc::ride::GetTripRequest* request,
                                         ::trpc::ride::GetTripReply* reply) {
  const std::string dht_key = DhtKeyOf(request->trip_id());
  const auto dht_proxy = GetDhtProxy();
  if (!dht_proxy) {
    reply->set_found(false);
    reply->set_message("dht entry node-a proxy is unavailable");
    return ::trpc::kSuccStatus;
  }

  // Lab 1B: Get("trip:" + trip_id) -> Trip.ParseFromString(value).
  ::trpc::dht::GetRequest get_request;
  get_request.set_key(dht_key);
  get_request.set_hops(0);
  ::trpc::dht::GetReply get_reply;
  const auto dht_context = ::trpc::MakeClientContext(context, dht_proxy);
  dht_context->SetFuncName("/trpc.dht.DhtService/Get");
  const ::trpc::Status get_status =
      dht_proxy->UnaryInvoke<::trpc::dht::GetRequest, ::trpc::dht::GetReply>(
          dht_context, get_request, &get_reply);
  if (!get_status.OK()) {
    reply->set_found(false);
    reply->set_message("dht get rpc failed: " + get_status.ErrorMessage());
    TRPC_FMT_WARN("get trip trip_id={} dht_key={} rpc_error={}", request->trip_id(), dht_key,
                  get_status.ErrorMessage());
    return ::trpc::kSuccStatus;
  }
  if (!get_reply.found()) {
    reply->set_found(false);
    reply->set_message("trip not found in dht: " + get_reply.message());
    TRPC_FMT_INFO("trip_id={} found=0 dht_key={} dht_owner={} dht_hops={}", request->trip_id(),
                  dht_key, get_reply.owner(), get_reply.hops());
    return ::trpc::kSuccStatus;
  }

  ::trpc::ride::Trip trip;
  if (!trip.ParseFromString(get_reply.value())) {
    reply->set_found(false);
    reply->set_message("dht value cannot be parsed as Trip");
    TRPC_FMT_ERROR("trip_id={} dht_key={} parse_failed bytes={}", request->trip_id(), dht_key,
                   get_reply.value().size());
    return ::trpc::kSuccStatus;
  }

  *reply->mutable_trip() = trip;
  reply->set_found(true);
  reply->set_message("trip read from dht through node-a entry");
  TRPC_FMT_INFO("trip_id={} found=1 dht_key={} dht_owner={} dht_hops={} bytes={}",
                request->trip_id(), dht_key, get_reply.owner(), get_reply.hops(),
                get_reply.value().size());
  return ::trpc::kSuccStatus;
}

}  // namespace lab1
