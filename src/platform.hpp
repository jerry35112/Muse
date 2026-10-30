#pragma once
#include "config.hpp"
#include <windows.h>
#include <filesystem>
namespace muse {
namespace fs=std::filesystem;
std::wstring wide(const std::string& s);
std::string utf8(const std::wstring& s);
std::string randomToken();
void trace(const char* message);
fs::path dataDir();
fs::path exeDir();
void writeFile(const fs::path& p,const std::string& bytes);
std::string readFile(const fs::path& p);
void saveProtected(const fs::path& p,const std::string& text);
std::string loadProtected(const fs::path& p);
bool isAdmin();
std::vector<std::string> processPaths();
std::vector<std::string> applicationPaths();
std::string pickProgram(HWND parent);
std::string http(int port,const std::string& secret,const std::wstring& verb,const std::string& path,const std::string& body="",int timeoutMs=2500);
class SystemProxy {
    fs::path backup_;
public:
    explicit SystemProxy(fs::path backup):backup_(std::move(backup)){}
    void enable(int port);
    void restore();
};
class Child {
    HANDLE process_=nullptr,job_=nullptr;
public:
    Child()=default;
    Child(const Child&)=delete;
    Child& operator=(const Child&)=delete;
    ~Child();
    void launch(const fs::path& exe,const fs::path& config,const fs::path& dir,bool test=false);
    bool alive() const;
    bool wait(DWORD timeout,DWORD& code);
    void stop();
};
}
