#include "engine.hpp"
#include "metadata.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
using namespace muse;
static int checks=0;
static void require(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
template<class Fn>static void rejects(Fn fn){bool rejected=false;try{fn();}catch(...){rejected=true;}require(rejected,"expected rejection");}
static Settings fixture(){Settings s;s.tun=false;s.subscriptions={{"abc123","Test","https://example.com/sub?token=private",true,3600},{"def456","Off","https://example.com/off",false,600}};return s;}
int main(int argc,char** argv){
    try{
        auto s=fixture();auto secret=randomToken();auto j=makeConfig(s,secret);
        require(secret.size()==48,"random secret");require(secret!=randomToken(),"randomness");
        require(j["external-controller"]=="127.0.0.1:19090","loopback controller");require(j["allow-lan"]==false,"no LAN");
        require(j["proxy-providers"].size()==1,"disabled providers excluded");require(j["proxy-groups"][0]["use"][0]=="sub_abc123","stable provider id");
        require(j["rules"].back()=="MATCH,PROXY","global routing");require(j["tun"]["enable"]==false,"system proxy mode");
        s.route=RouteMode::Selected;require(makeConfig(s,secret)["rules"].back()=="MATCH,PROXY","stale manual mode cannot override empty selection");
        s.processes={"C:\\Program Files\\Example\\test.exe","D:\\应用\\浏览器.exe"};j=makeConfig(s,secret);
        require(j["tun"]["enable"]==true,"process selection forces TUN");require(j["rules"].back()=="MATCH,DIRECT","other applications direct");
        require(j["rules"][8]=="PROCESS-PATH,C:\\Program Files\\Example\\test.exe,PROXY","path preserved");
        require(j["rules"][9]=="PROCESS-PATH,D:\\应用\\浏览器.exe,PROXY","unicode preserved");
        require(j["find-process-mode"]=="always","process detection");require(j["tun"]["strict-route"]==true,"strict route");require(j["ipv6"]==true,"capture ipv6");
        require(serialize(deserialize(serialize(s)))==serialize(s),"settings round trip");
        s.route=RouteMode::All;require(makeConfig(s,secret)["rules"].back()=="MATCH,DIRECT","selection overrides stale global mode");
        auto single=s;single.processes.resize(1);require(makeConfig(single,secret)["rules"].size()==10,"single program rule");
        single.processes.clear();auto global=makeConfig(single,secret);require(global["rules"].back()=="MATCH,PROXY","deselect last restores global");require(global["find-process-mode"]=="strict","global detection mode");
        Settings fresh;require(fresh.tun,"fresh installations default to full TUN capture");
        auto legacy=serialize(single);legacy["version"]=1;legacy["route"]="selected";legacy["tun"]=false;auto migrated=deserialize(legacy);require(effectiveRoute(migrated)==RouteMode::All&&migrated.tun,"legacy settings migrate to automatic global");
        auto usage=subscriptionUsage({{"Total",1000},{"Upload",100},{"Download",250},{"Expire",1800000000}});
        require(usage.known&&usage.remaining==650&&usage.used==350&&usage.fraction==.35&&usage.expires==1800000000,"subscription usage matches provider header");
        require(!subscriptionUsage(Json()).known,"missing quota stays unknown");require(!subscriptionUsage({{"Total",0}}).known,"zero total is not invented quota");
        usage=subscriptionUsage({{"Total",100},{"Upload",200}});require(usage.remaining==0&&usage.fraction==1,"over quota clamped");
        require(!subscriptionUsage({{"Total","invalid"},{"Download",nullptr}}).known,"malformed provider info safe");
        require(latestDelay(Json::array())==-1,"not measured differs from timeout");require(latestDelay(Json::array({{{"delay",0}}}))==0,"timeout history");require(latestDelay(Json::array({{{"delay",100}},{{"delay",42}}}))==42,"newest real latency");
        require(latestDelay(Json::array({{{"delay",nullptr}}}))==-1,"malformed latency safe");
        auto names=Json::array({"A","B","C"});
        require(fastestNode(names,{{"A",120},{"B",34},{"C",0},{"unknown",1}},"A")=="B","fastest only considers current usable nodes");
        require(fastestNode(names,{{"A",40},{"B",40},{"C",0}},"B")=="B","equal latency retains current choice");
        require(fastestNode(names,{{"A",0},{"B",-1},{"C",nullptr}},"A").empty(),"all timeouts never select a zero-latency node");
        require(fastestNode(names,{{"A",65536},{"B","20"}},"A").empty(),"malformed delays not selectable");
        auto evil=s;evil.processes={"C:\\bad.exe,DIRECT\nMATCH,DIRECT"};rejects([&]{makeConfig(evil,secret);});
        evil=s;evil.mixedPort=evil.apiPort;rejects([&]{validate(evil);});evil=s;evil.apiPort=65536;rejects([&]{validate(evil);});
        evil=s;evil.subscriptions[0].url="http://example.com";rejects([&]{validate(evil);});
        evil=s;evil.subscriptions[0].enabled=false;rejects([&]{validate(evil);});
        evil=s;evil.subscriptions[1].id=evil.subscriptions[0].id;rejects([&]{validate(evil);});
        rejects([&]{makeConfig(s,"weak");});
        auto path=exeDir()/wide("storage-test-"+randomToken()+".dat");
        auto payload=serialize(s).dump();saveProtected(path,payload);
        require(readFile(path).find("token=private")==std::string::npos,"encrypted subscription at rest");require(loadProtected(path)==payload,"DPAPI round trip");fs::remove(path);
        auto paths=processPaths();require(!paths.empty(),"process enumeration");require(utf8(wide("Muse 测试"))=="Muse 测试","UTF-8 round trip");
        if(argc==3 && std::string(argv[1])=="--emit-config")writeFile(wide(argv[2]),makeConfig(s,secret).dump(2));
        if(argc==3 && std::string(argv[1])=="--core") {
            auto dir=exeDir()/wide("core-test-"+randomToken());fs::create_directories(dir);
            auto config=makeConfig(fixture(),secret);
            config["external-controller"]="127.0.0.1:29181";config["mixed-port"]=27891;config["dns"]["enable"]=false;
            config["proxy-providers"]={{"sub_abc123",{{"type","file"},{"path","./nodes.json"},{"health-check",{{"enable",false}}}}}};
            Json proxies=Json::array();for(auto name:{"LOCAL-A","LOCAL-B"})proxies.push_back({{"name",name},{"type","socks5"},{"server","127.0.0.1"},{"port",9}});
            writeFile(dir/L"nodes.json",Json{{"proxies",proxies}}.dump());writeFile(dir/L"config.json",config.dump());
            Child child;child.launch(fs::absolute(wide(argv[2])),dir/L"config.json",dir,true);DWORD code=1;
            require(child.wait(10000,code)&&code==0,"mihomo validates generated config");
            child.launch(fs::absolute(wide(argv[2])),dir/L"config.json",dir);
            bool ready=false;for(int i=0;i<30;i++) {
                try{auto version=Json::parse(http(29181,secret,L"GET","/version"));require(version.contains("version"),"core version");ready=true;break;}catch(...){}
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            require(ready,"core controller startup");
            auto group=Json::parse(http(29181,secret,L"GET","/proxies/PROXY"));require(group["all"].size()==2,"file subscription loaded");
            http(29181,secret,L"PUT","/proxies/PROXY",Json{{"name","LOCAL-B"}}.dump());
            group=Json::parse(http(29181,secret,L"GET","/proxies/PROXY"));require(group["now"]=="LOCAL-B","node switching");
            http(29181,secret,L"PUT","/providers/proxies/sub_abc123");require(child.alive(),"provider update");
            rejects([&]{http(29181,"incorrect",L"GET","/version");});
            child.stop();require(!child.alive(),"child shutdown");
            config["tun"]["enable"]=true;config["find-process-mode"]="always";config["rules"].insert(config["rules"].end()-1,"PROCESS-PATH,C:\\Program Files\\Example\\test.exe,PROXY");
            writeFile(dir/L"config.json",config.dump());child.launch(fs::absolute(wide(argv[2])),dir/L"config.json",dir,true);
            require(child.wait(10000,code)&&code==0,"mihomo validates TUN and process rules");child.stop();
            // Remove only known files in this test-owned directory.
            for(auto& f:fs::directory_iterator(dir))if(f.is_regular_file())fs::remove(f.path());
            std::error_code ec;fs::remove(dir,ec);
        }
        std::cout<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
