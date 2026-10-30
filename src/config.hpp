#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace muse {
using Json = nlohmann::json;
struct Subscription {
    std::string id, name, url;
    bool enabled = true;
    int interval = 3600;
};
enum class RouteMode { All, Selected };
struct Settings {
    std::vector<Subscription> subscriptions;
    std::vector<std::string> processes;
    std::vector<std::string> libraryPaths;
    RouteMode route = RouteMode::All;
    bool tun = true;
    int mixedPort = 17890, apiPort = 19090;
};
Json serialize(const Settings& s);
Settings deserialize(const Json& j);
void validate(const Settings& s);
Json makeConfig(const Settings& s, const std::string& secret);
std::string providerKey(const Subscription& s);
RouteMode effectiveRoute(const Settings& s);
}
