#include "src/pricing_service.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <string>

#include "trpc/common/status.h"
#include "trpc/log/trpc_log.h"

namespace lab1 {

namespace {

constexpr double kBaseFare = 10.0;
constexpr double kDistanceFarePerKm = 2.4;
constexpr double kPeakMultiplier = 1.35;
constexpr double kOffPeakMultiplier = 1.0;

bool IsBlank(const std::string& value) {
  return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

std::string QuotedAtUtc() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm tm_utc{};
  gmtime_r(&now_time, &tm_utc);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return std::string(buffer);
}

}  // namespace

::trpc::Status PricingServiceImpl::EstimateFare(
    ::trpc::ServerContextPtr context, const ::trpc::ride::EstimateFareRequest* request,
    ::trpc::ride::EstimateFareReply* reply) {
  if (IsBlank(request->city())) {
    reply->set_ok(false);
    reply->set_message("city is required");
    TRPC_FMT_WARN("estimate fare rejected: empty city");
    return ::trpc::kSuccStatus;
  }
  if (!std::isfinite(request->distance_km()) || request->distance_km() <= 0.0) {
    reply->set_ok(false);
    reply->set_message("distance_km must be a finite positive number");
    TRPC_FMT_WARN("estimate fare rejected city={} distance_km={}",
                  request->city(), request->distance_km());
    return ::trpc::kSuccStatus;
  }

  double multiplier;
  if (request->time_slot() == "peak") {
    multiplier = kPeakMultiplier;
  } else if (request->time_slot() == "offpeak") {
    multiplier = kOffPeakMultiplier;
  } else {
    reply->set_ok(false);
    reply->set_message("time_slot must be 'peak' or 'offpeak'");
    TRPC_FMT_WARN("estimate fare rejected city={} time_slot={}",
                  request->city(), request->time_slot());
    return ::trpc::kSuccStatus;
  }

  // The offline policy below is the single source of truth for all fare quotes.
  const double base_fare = kBaseFare;
  const double distance_fare = request->distance_km() * kDistanceFarePerKm;
  const double total_fare = (base_fare + distance_fare) * multiplier;

  reply->set_ok(true);
  reply->set_base_fare(base_fare);
  reply->set_distance_fare(distance_fare);
  reply->set_multiplier(multiplier);
  reply->set_total_fare(total_fare);
  reply->set_quoted_at(QuotedAtUtc());
  reply->set_message("fare computed by offline teaching rule");
  TRPC_FMT_INFO(
      "city={} distance_km={} time_slot={} base_fare={} distance_fare={} multiplier={} total_fare={}",
      request->city(), request->distance_km(), request->time_slot(),
      base_fare, distance_fare, multiplier, total_fare);
  return ::trpc::kSuccStatus;
}

}  // namespace lab1
