#include "metadata.hpp"
#include <cmath>
#include <algorithm>
namespace muse {
static double number(const Json& j,const char* key){if(!j.is_object()||!j.contains(key)||!j[key].is_number())return 0;auto n=j[key].get<double>();return std::isfinite(n)&&n>=0?n:0;}
SubscriptionUsage subscriptionUsage(const Json& j){
    SubscriptionUsage u;u.total=number(j,"Total");u.used=number(j,"Upload")+number(j,"Download");
    if(!std::isfinite(u.used))u.used=0;
    auto expiry=number(j,"Expire");if(expiry>0&&expiry<253402300800.0)u.expires=static_cast<int64_t>(expiry);
    if(u.total>0){u.known=true;u.remaining=std::max(0.0,u.total-u.used);u.fraction=std::clamp(u.used/u.total,0.0,1.0);}return u;
}
int latestDelay(const Json& history){
    if(!history.is_array()||history.empty()||!history.back().is_object())return -1;
    auto& entry=history.back();if(!entry.contains("delay")||!entry["delay"].is_number_integer())return -1;
    auto n=entry["delay"].get<int64_t>();return n>=0&&n<=65535?static_cast<int>(n):-1;
}
std::string fastestNode(const Json& names,const Json& delays,const std::string& current){
    if(!names.is_array()||!delays.is_object())return {};
    std::string best;int minimum=65536;
    for(auto& item:names){
        if(!item.is_string())continue;auto name=item.get<std::string>();
        if(name=="DIRECT"||name=="REJECT"||name=="COMPATIBLE"||!delays.contains(name)||!delays[name].is_number_integer())continue;
        auto value=delays[name].get<int64_t>();if(value<=0||value>65535)continue;
        if(value<minimum||(value==minimum&&name==current)){minimum=static_cast<int>(value);best=name;}
    }return best;
}
}
