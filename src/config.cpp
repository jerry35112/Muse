#include "config.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace muse {
std::string providerKey(const Subscription& s) { return "sub_" + s.id; }
RouteMode effectiveRoute(const Settings& s){return s.processes.empty()?RouteMode::All:RouteMode::Selected;}
Json serialize(const Settings& s) {
    Json subs = Json::array();
    for (auto& x : s.subscriptions) subs.push_back({{"id",x.id},{"name",x.name},{"url",x.url},{"enabled",x.enabled},{"interval",x.interval}});
    return {{"version",2},{"subscriptions",subs},{"processes",s.processes},{"library_paths",s.libraryPaths},{"route",effectiveRoute(s) == RouteMode::All ? "all" : "selected"},{"tun",s.tun},{"mixed_port",s.mixedPort},{"api_port",s.apiPort}};
}
Settings deserialize(const Json& j) {
    auto version=j.value("version",0);
    if(version!=1 && version!=2) throw std::runtime_error("不支持的设置版本");
    Settings s;
    for(auto& x:j.at("subscriptions")) s.subscriptions.push_back({x.at("id"),x.at("name"),x.at("url"),x.value("enabled",true),x.value("interval",3600)});
    s.processes=j.value("processes",std::vector<std::string>{});
    s.libraryPaths=j.value("library_paths",std::vector<std::string>{});
    auto route=j.value("route",std::string("all"));
    if(route!="all" && route!="selected") throw std::runtime_error("无效的路由模式");
    s.route=effectiveRoute(s);
    s.tun=version==1?true:j.value("tun",true); s.mixedPort=j.value("mixed_port",17890); s.apiPort=j.value("api_port",19090);
    return s;
}
void validate(const Settings& s) {
    if(s.mixedPort<1024 || s.mixedPort>65535 || s.apiPort<1024 || s.apiPort>65535 || s.mixedPort==s.apiPort)
        throw std::runtime_error("端口须在 1024–65535 之间，且不能相同");
    std::set<std::string> ids;
    size_t count=0;
    for(auto& x:s.subscriptions) {
        if(x.id.empty() || !std::all_of(x.id.begin(),x.id.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}) || !ids.insert(x.id).second)
            throw std::runtime_error("订阅标识无效或重复");
        if(x.name.empty() || x.name.size()>256) throw std::runtime_error("请填写订阅名称");
        if(x.url.rfind("https://",0)!=0 || x.url.size()<10 || x.url.find_first_of("\r\n\t ")!=std::string::npos)
            throw std::runtime_error("订阅地址必须是完整的 HTTPS 链接");
        if(x.interval<300 || x.interval>604800) throw std::runtime_error("订阅更新间隔须为 300–604800 秒");
        count+=x.enabled;
    }
    if(!count) throw std::runtime_error("请至少添加并启用一个订阅");
    for(auto& p:s.processes) {
        // Commas delimit mihomo rules; reject instead of silently changing the rule.
        if(p.size()<4 || p.find_first_of(",\r\n")!=std::string::npos || p.find('\0')!=std::string::npos || !(p.size()>2 && p[1]==':'))
            throw std::runtime_error("程序路径必须为完整的 Windows 路径，且不能包含逗号或换行");
    }
}
Json makeConfig(const Settings& s,const std::string& secret) {
    validate(s);
    if(secret.size()<32) throw std::runtime_error("控制接口密钥过短");
    auto route=effectiveRoute(s);
    bool tun=s.tun || route==RouteMode::Selected;
    Json providers=Json::object(), use=Json::array(), rules=Json::array();
    for(auto& x:s.subscriptions) if(x.enabled) {
        auto key=providerKey(x); use.push_back(key);
        providers[key]={{"type","http"},{"url",x.url},{"path","./providers/"+key+".yaml"},{"interval",x.interval},{"size-limit",10485760},
            {"health-check",{{"enable",true},{"url","https://www.gstatic.com/generate_204"},{"interval",600},{"lazy",true}}}};
    }
    // The core's own outbound sockets bypass interception. Also keep local destinations direct.
    for(auto cidr:{"127.0.0.0/8","10.0.0.0/8","172.16.0.0/12","192.168.0.0/16","169.254.0.0/16"})
        rules.push_back(std::string("IP-CIDR,")+cidr+",DIRECT,no-resolve");
    for(auto cidr:{"::1/128","fc00::/7","fe80::/10"}) rules.push_back(std::string("IP-CIDR6,")+cidr+",DIRECT,no-resolve");
    if(route==RouteMode::Selected) {
        for(auto& p:s.processes) rules.push_back("PROCESS-PATH,"+p+",PROXY");
        rules.push_back("MATCH,DIRECT");
    } else rules.push_back("MATCH,PROXY");
    return {{"mixed-port",s.mixedPort},{"allow-lan",false},{"bind-address","127.0.0.1"},{"mode","rule"},
        {"log-level","warning"},{"ipv6",true},{"find-process-mode",route==RouteMode::Selected ? "always":"strict"},
        {"external-controller","127.0.0.1:"+std::to_string(s.apiPort)},{"secret",secret},
        {"profile",{{"store-selected",false},{"store-fake-ip",false}}},
        {"tun",{{"enable",tun},{"device","MuseTun"},{"stack","gvisor"},{"auto-route",true},{"auto-detect-interface",true},{"strict-route",true},{"dns-hijack",Json::array({"any:53","tcp://any:53"})}}},
        {"dns",{{"enable",true},{"listen","127.0.0.1:0"},{"ipv6",true},{"enhanced-mode","fake-ip"},{"fake-ip-range","198.18.0.1/16"},
            {"default-nameserver",Json::array({"223.5.5.5","1.1.1.1"})},{"nameserver",Json::array({"https://dns.alidns.com/dns-query","https://cloudflare-dns.com/dns-query"})},
            {"proxy-server-nameserver",Json::array({"https://dns.alidns.com/dns-query"})}}},
        {"proxy-providers",providers},{"proxy-groups",Json::array({{{"name","PROXY"},{"type","select"},{"use",use}}})},{"rules",rules}};
}
}
