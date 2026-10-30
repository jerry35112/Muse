#pragma once
#include "platform.hpp"
#include <atomic>
namespace muse {
class Engine {
    Child child_;
    fs::path dir_,runtime_;
    SystemProxy proxy_;
    std::string secret_;
    int port_=0;
    bool active_=false;
public:
    explicit Engine(fs::path dir):dir_(dir),runtime_(dir/L"runtime.json"),proxy_(dir/L"proxy-backup.dat"){}
    ~Engine();
    void recover();
    void start(const Settings& s,bool activate=true,const std::string& preferredNode={},const std::atomic_bool* cancel=nullptr);
    void stop();
    void deactivate();
    bool alive()const{return child_.alive();}
    bool proxyActive()const{return active_&&alive();}
    bool needsCleanup()const{return port_!=0&&!alive();}
    Json request(const std::wstring& verb,const std::string& path,const Json* body=nullptr,int timeoutMs=2500);
};
}
