#include "platform.hpp"
#include <winhttp.h>
#include <wininet.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace muse {
static void check(bool ok,const char* what) { if(!ok) throw std::runtime_error(std::string(what)+" ("+std::to_string(GetLastError())+")"); }
std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    check(n>0,"UTF-8 转换失败"); std::wstring out(n,0);
    MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n); return out;
}
std::string utf8(const std::wstring& s) {
    if(s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr); return out;
}
std::string randomToken() {
    unsigned char bytes[24];
    if(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0) throw std::runtime_error("无法生成安全随机数");
    std::string out; for(auto b:bytes) {out+="0123456789abcdef"[b>>4]; out+="0123456789abcdef"[b&15];} return out;
}
fs::path exeDir() { std::wstring p(32768,0); auto n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size())); check(n>0&&n<p.size(),"无法定位程序"); p.resize(n); return fs::path(p).parent_path(); }
// Opt-in startup/teardown tracing: set MUSE_TRACE=1 to get a line-by-line log next
// to the executable. Used to find hangs that only show up on real desktops.
void trace(const char* message) {
    wchar_t flag[8]{};
    if(GetEnvironmentVariableW(L"MUSE_TRACE",flag,8)!=1||flag[0]!=L'1')return;
    try{std::ofstream out(exeDir()/L"muse-trace.log",std::ios::binary|std::ios::app);
        out<<static_cast<double>(GetTickCount64())/1000.0<<" "<<message<<"\n";}catch(...){}
}
fs::path dataDir() {
    auto path=exeDir()/L"data";
    // Owner and administrators only; provider caches also contain credentials.
    PSECURITY_DESCRIPTOR sd=nullptr;
    check(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)",SDDL_REVISION_1,&sd,nullptr)!=0,"无法创建目录权限");
    SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
    BOOL ok=CreateDirectoryW(path.c_str(),&sa); auto err=GetLastError(); LocalFree(sd);
    if(!ok && err!=ERROR_ALREADY_EXISTS) throw std::runtime_error("无法创建设置目录");
    return path;
}
void writeFile(const fs::path& p,const std::string& bytes) {
    auto tmp=p; tmp+=L".tmp";
    {std::ofstream f(tmp,std::ios::binary|std::ios::trunc); if(!f || !f.write(bytes.data(),bytes.size())) throw std::runtime_error("文件写入失败");}
    check(MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"文件保存失败");
}
std::string readFile(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); if(!f) throw std::runtime_error("文件读取失败");
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
void saveProtected(const fs::path& p,const std::string& text) {
    DATA_BLOB in{static_cast<DWORD>(text.size()),reinterpret_cast<BYTE*>(const_cast<char*>(text.data()))},out{};
    check(CryptProtectData(&in,L"Muse settings",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)!=0,"设置加密失败");
    std::string bytes(reinterpret_cast<char*>(out.pbData),out.cbData); LocalFree(out.pbData); writeFile(p,bytes);
}
std::string loadProtected(const fs::path& p) {
    auto bytes=readFile(p); DATA_BLOB in{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},out{};
    check(CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)!=0,"设置解密失败，请使用原 Windows 账户");
    std::string text(reinterpret_cast<char*>(out.pbData),out.cbData); LocalFree(out.pbData); return text;
}
bool isAdmin() { SID_IDENTIFIER_AUTHORITY nt=SECURITY_NT_AUTHORITY; PSID sid=nullptr; BOOL admin=FALSE;
    if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)) {CheckTokenMembership(nullptr,sid,&admin); FreeSid(sid);} return admin!=FALSE;
}
std::vector<std::string> processPaths() {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); check(snap!=INVALID_HANDLE_VALUE,"无法读取进程列表");
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry); std::set<std::string> paths;
    if(Process32FirstW(snap,&entry)) do {
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if(p) {wchar_t buf[32768]; DWORD n=32768; if(QueryFullProcessImageNameW(p,0,buf,&n)) paths.insert(utf8(std::wstring(buf,n))); CloseHandle(p);}
    } while(Process32NextW(snap,&entry));
    CloseHandle(snap); return {paths.begin(),paths.end()};
}
std::vector<std::string> applicationPaths(){
    // Match desktop applications, rather than every executable in the process table.
    std::set<DWORD> services;
    if(auto scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ENUMERATE_SERVICE)){
        DWORD bytes=0,count=0,resume=0;
        EnumServicesStatusExW(scm,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_ACTIVE,nullptr,0,&bytes,&count,&resume,nullptr);
        if(bytes){std::vector<BYTE> buffer(bytes);resume=0;
            if(EnumServicesStatusExW(scm,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_ACTIVE,buffer.data(),bytes,&bytes,&count,&resume,nullptr)){
                auto* records=reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());for(DWORD i=0;i<count;++i)services.insert(records[i].ServiceStatusProcess.dwProcessId);
            }
        }CloseServiceHandle(scm);
    }
    struct Context{std::set<DWORD>* services;std::set<std::wstring> paths;};Context context{&services,{}};
    check(EnumWindows([](HWND window,LPARAM data)->BOOL{
        auto& ctx=*reinterpret_cast<Context*>(data);
        if(!IsWindowVisible(window)||GetWindow(window,GW_OWNER)||GetWindowTextLengthW(window)==0)return TRUE;
        auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);if((style&WS_EX_TOOLWINDOW)&&!(style&WS_EX_APPWINDOW))return TRUE;
        DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(!pid||pid==GetCurrentProcessId()||ctx.services->contains(pid))return TRUE;
        if(auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)){
            wchar_t path[32768];DWORD size=32768;
            if(QueryFullProcessImageNameW(process,0,path,&size))ctx.paths.insert(std::wstring(path,size));CloseHandle(process);
        }return TRUE;
    },reinterpret_cast<LPARAM>(&context))!=0,"无法读取应用窗口列表");
    std::vector<std::string> paths;for(auto& path:context.paths)paths.push_back(utf8(path));return paths;
}
std::string pickProgram(HWND parent) {
    wchar_t buf[32768]{}; OPENFILENAMEW ofn{sizeof(ofn)}; ofn.hwndOwner=parent; ofn.lpstrFile=buf; ofn.nMaxFile=32768;
    ofn.lpstrFilter=L"Applications (*.exe)\0*.exe\0\0"; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? utf8(buf) : "";
}
struct InternetHandle { HINTERNET h; ~InternetHandle(){if(h)WinHttpCloseHandle(h);} operator HINTERNET()const{return h;} };
// The control socket is loopback-only, so no proxy is consulted for it either.
static std::wstring userAgent(){ return wide(std::string("Muse/") + MUSE_VERSION); }
std::string http(int port,const std::string& secret,const std::wstring& verb,const std::string& path,const std::string& body,int timeoutMs) {
    InternetHandle session{WinHttpOpen(userAgent().c_str(),WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    check(session.h!=nullptr,"控制接口初始化失败"); WinHttpSetTimeouts(session,1500,1500,2500,timeoutMs);
    InternetHandle conn{WinHttpConnect(session,L"127.0.0.1",static_cast<INTERNET_PORT>(port),0)}; check(conn.h!=nullptr,"控制接口连接失败");
    InternetHandle req{WinHttpOpenRequest(conn,verb.c_str(),wide(path).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0)}; check(req.h!=nullptr,"控制请求创建失败");
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER; WinHttpSetOption(req,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    auto headers=wide("Authorization: Bearer "+secret+"\r\nContent-Type: application/json\r\n");
    check(WinHttpSendRequest(req,headers.c_str(),static_cast<DWORD>(-1),body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0)!=0,"内核没有响应");
    check(WinHttpReceiveResponse(req,nullptr)!=0,"内核响应失败");
    DWORD status=0,n=sizeof(status); check(WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&n,nullptr)!=0,"无法读取响应状态");
    if(status<200 || status>=300) throw std::runtime_error("控制接口请求失败 HTTP "+std::to_string(status));
    std::string out; char buf[8192]; DWORD got=0;
    do {check(WinHttpReadData(req,buf,sizeof(buf),&got)!=0,"无法读取响应"); out.append(buf,got); if(out.size()>8*1024*1024) throw std::runtime_error("控制响应过大");} while(got);
    return out;
}
static Json queryProxy() {
    INTERNET_PER_CONN_OPTIONW opt[4]{};
    opt[0].dwOption=INTERNET_PER_CONN_FLAGS; opt[1].dwOption=INTERNET_PER_CONN_PROXY_SERVER;
    opt[2].dwOption=INTERNET_PER_CONN_PROXY_BYPASS; opt[3].dwOption=INTERNET_PER_CONN_AUTOCONFIG_URL;
    INTERNET_PER_CONN_OPTION_LISTW list{sizeof(list),nullptr,4,0,opt}; DWORD size=sizeof(list);
    check(InternetQueryOptionW(nullptr,INTERNET_OPTION_PER_CONNECTION_OPTION,&list,&size)!=0,"无法读取系统代理");
    Json j={{"flags",opt[0].Value.dwValue}};
    const char* keys[]={"","server","bypass","pac"};
    for(int i=1;i<4;i++) {j[keys[i]]=opt[i].Value.pszValue ? utf8(opt[i].Value.pszValue):""; if(opt[i].Value.pszValue) GlobalFree(opt[i].Value.pszValue);}
    return j;
}
static void applyProxy(const Json& j) {
    auto server=wide(j.at("server")),bypass=wide(j.at("bypass")),pac=wide(j.at("pac"));
    INTERNET_PER_CONN_OPTIONW opt[4]{};
    opt[0].dwOption=INTERNET_PER_CONN_FLAGS; opt[0].Value.dwValue=j.at("flags").get<DWORD>();
    opt[1].dwOption=INTERNET_PER_CONN_PROXY_SERVER; opt[1].Value.pszValue=server.data();
    opt[2].dwOption=INTERNET_PER_CONN_PROXY_BYPASS; opt[2].Value.pszValue=bypass.data();
    opt[3].dwOption=INTERNET_PER_CONN_AUTOCONFIG_URL; opt[3].Value.pszValue=pac.data();
    INTERNET_PER_CONN_OPTION_LISTW list{sizeof(list),nullptr,4,0,opt};
    check(InternetSetOptionW(nullptr,INTERNET_OPTION_PER_CONNECTION_OPTION,&list,sizeof(list))!=0,"无法修改系统代理");
    InternetSetOptionW(nullptr,INTERNET_OPTION_SETTINGS_CHANGED,nullptr,0); InternetSetOptionW(nullptr,INTERNET_OPTION_REFRESH,nullptr,0);
}
void SystemProxy::enable(int port) {
    if(fs::exists(backup_)) throw std::runtime_error("存在待恢复的系统代理，请先恢复");
    auto old=queryProxy(); auto server="127.0.0.1:"+std::to_string(port);
    saveProtected(backup_,Json{{"old",old},{"owned",server}}.dump());
    applyProxy({{"flags",PROXY_TYPE_DIRECT|PROXY_TYPE_PROXY},{"server",server},{"bypass","localhost;127.*;[::1];<local>"},{"pac",""}});
}
void SystemProxy::restore() {
    if(!fs::exists(backup_)) return;
    auto saved=Json::parse(loadProtected(backup_)); auto now=queryProxy();
    // Respect a proxy change made by the user or another client during this session.
    if(now.at("server")==saved.at("owned") && now.at("flags")==DWORD(PROXY_TYPE_DIRECT|PROXY_TYPE_PROXY) && now.at("pac")=="") applyProxy(saved.at("old"));
    fs::remove(backup_);
}
Child::~Child(){stop();}
void Child::launch(const fs::path& exe,const fs::path& config,const fs::path& dir,bool test) {
    stop();
    auto cmd=L"\""+exe.wstring()+L"\" -d \""+dir.wstring()+L"\" -f \""+config.wstring()+L"\""+(test?L" -t":L"");
    job_=CreateJobObjectW(nullptr,nullptr); check(job_!=nullptr,"无法创建内核任务");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{}; limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    check(SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limit,sizeof(limit))!=0,"无法设置内核生命周期");
    STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
    check(CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,dir.c_str(),&si,&pi)!=0,"内核启动失败");
    if(!AssignProcessToJobObject(job_,pi.hProcess)) {TerminateProcess(pi.hProcess,1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); throw std::runtime_error("内核任务绑定失败");}
    process_=pi.hProcess; ResumeThread(pi.hThread); CloseHandle(pi.hThread);
}
bool Child::alive()const{return process_ && WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
bool Child::wait(DWORD timeout,DWORD& code) { if(!process_ || WaitForSingleObject(process_,timeout)!=WAIT_OBJECT_0) return false; return GetExitCodeProcess(process_,&code)!=0; }
void Child::stop() {
    if(process_ && alive()) {TerminateProcess(process_,0); WaitForSingleObject(process_,3000);}
    if(process_) CloseHandle(process_); if(job_) CloseHandle(job_); process_=job_=nullptr;
}
}
