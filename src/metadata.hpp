#pragma once
#include "config.hpp"
namespace muse {
struct SubscriptionUsage {bool known=false;double total=0,used=0,remaining=0,fraction=0;int64_t expires=0;};
SubscriptionUsage subscriptionUsage(const Json& info);
int latestDelay(const Json& history);
std::string fastestNode(const Json& names,const Json& delays,const std::string& current);
}
