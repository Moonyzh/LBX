#include "src/trip_service.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

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

std::string TripServiceImpl::NextTripId() {
  std::lock_guard<std::mutex> lock(trips_mutex_);
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
  trip.set_trip_id(NextTripId());
  trip.set_passenger_id(request->passenger_id());
  trip.set_city(request->city());
  trip.set_origin(request->origin());
  trip.set_destination(request->destination());
  trip.set_distance_km(request->distance_km());
  trip.set_time_slot(request->time_slot());
  trip.set_estimated_fare(pricing_reply.total_fare());
  trip.set_status("WAITING");
  {
    std::lock_guard<std::mutex> lock(trips_mutex_);
    trips_[trip.trip_id()] = trip;
  }
  *reply->mutable_trip() = trip;
  reply->set_ok(true);
  reply->set_message("trip created after pricing-service quote");
  TRPC_FMT_INFO("trip_id={} passenger_id={} downstream=pricing-service estimated_fare={}",
                trip.trip_id(), trip.passenger_id(), trip.estimated_fare());
  return ::trpc::kSuccStatus;
}

::trpc::Status TripServiceImpl::GetTrip(::trpc::ServerContextPtr context,
                                         const ::trpc::ride::GetTripRequest* request,
                                         ::trpc::ride::GetTripReply* reply) {
  std::lock_guard<std::mutex> lock(trips_mutex_);
  const auto it = trips_.find(request->trip_id());
  reply->set_found(it != trips_.end());
  if (it != trips_.end()) {
    *reply->mutable_trip() = it->second;
    reply->set_message("trip read from local teaching repository");
  } else {
    reply->set_message("trip not found");
  }
  TRPC_FMT_INFO("trip_id={} found={}", request->trip_id(), it != trips_.end());
  return ::trpc::kSuccStatus;
}

}  // namespace lab1
