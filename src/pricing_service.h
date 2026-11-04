#pragma once

#include "proto/pricing.trpc.pb.h"

namespace lab1 {

// PricingService is the single owner of the fare policy. The implementation
// is intentionally offline and deterministic: no map, ride-hailing or
// payment API is called.
class PricingServiceImpl final : public ::trpc::ride::PricingService {
 public:
  ::trpc::Status EstimateFare(::trpc::ServerContextPtr context,
                              const ::trpc::ride::EstimateFareRequest* request,
                              ::trpc::ride::EstimateFareReply* reply) override;
};

}  // namespace lab1
