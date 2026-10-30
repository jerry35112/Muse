#include "platform.hpp"
#include "app_library.hpp"
#include "app_catalog.hpp"
#include "app_icons.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <chrono>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <wrl/client.h>
namespace muse {
using Microsoft::WRL::ComPtr;
struct ComScope {HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);~ComScope(){if(SUCCEEDED(result))CoUninitialize();}};
static QString normalized(QString path){return QDir::toNativeSeparators(QDir::cleanPath(path));}
QString shortcutExecutable(const QString& shortcut){
    ComScope com;ComPtr<IShellLinkW> link;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))))return {};
    ComPtr<IPersistFile> file;if(FAILED(link.As(&file))||FAILED(file->Load(shortcut.toStdWString().c_str(),STGM_READ)))return {};
    wchar_t target[32768]{};
    // Read the link target only. Never execute it or invoke interactive shortcut resolution.
    if(FAILED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH))||!target[0])return {};
    wchar_t expanded[32768]{};auto count=ExpandEnvironmentStringsW(target,expanded,32768);if(!count||count>32768)return {};
    auto path=normalized(QString::fromWCharArray(expanded));
    if(path.size()<4||path[1]!=':'||!QFileInfo(path).isAbsolute()||!path.endsWith(".exe",Qt::CaseInsensitive)||!QFileInfo(path).isFile())return {};
    return path;
}
QVariantList scanApplicationLibrary(const QStringList& roots,const QStringList& registered,const QStringList& running,const std::atomic_bool* cancel,bool* complete){
    auto stopped=[&]{return cancel&&cancel->load();};
    // Shell calls on shortcuts can block for a long time (network targets, cloud
    // placeholders). The scan is best effort: stop after the budget is spent and
    // report that the result is partial so callers do not cache it.
    const auto started=std::chrono::steady_clock::now();
    const auto expired=[&]{return std::chrono::steady_clock::now()-started>std::chrono::seconds(20);};
    QMap<QString,QVariantMap> entries;
    auto add=[&](QString path,QString label,bool installed,bool active){
        path=normalized(path);auto lower=path.toCaseFolded();auto filename=QFileInfo(path).fileName();
        if(path.size()<4||path[1]!=':'||backgroundApplication(path)||!path.endsWith(".exe",Qt::CaseInsensitive)||!QFileInfo(path).isFile())return;
        auto utility=(filename+" "+label).toLower();for(auto word:{"uninstall","unins000","卸载","帮助文档","readme","setup.exe","maintenance.exe"})if(utility.contains(QString::fromUtf8(word)))return;
        if(entries.contains(lower)){entries[lower]["installed"]=entries[lower]["installed"].toBool()||installed;entries[lower]["running"]=entries[lower]["running"].toBool()||active;return;}
        auto name=label.trimmed();if(name.isEmpty())name=applicationName(path);
        entries[lower]={{"path",path},{"name",name},{"filename",filename},{"installed",installed},{"running",active},{"manual",false},{"background",false},{"category",applicationCategory(path,name)},{"icon",QString::fromLatin1(path.toUtf8().toHex())}};
    };
    for(auto& root:roots){
        // Qt classifies Windows .lnk files as links; NoSymLinks would discard the app shortcuts themselves.
        QDirIterator it(root,{"*.lnk"},QDir::Files,QDirIterator::Subdirectories);
        while(it.hasNext()&&!stopped()&&!expired()){auto shortcut=it.next();auto target=shortcutExecutable(shortcut);if(!target.isEmpty())add(target,QFileInfo(shortcut).completeBaseName(),true,false);}
        if(stopped()||expired())break;
    }
    for(auto& path:registered){if(stopped()||expired())break;add(path,{},true,false);}
    for(auto& path:running){if(stopped()||expired())break;add(path,{},false,true);}
    if(complete)*complete=!stopped()&&!expired();
    QVariantList rows;for(auto& entry:entries)rows.push_back(entry);
    std::sort(rows.begin(),rows.end(),[](const QVariant& a,const QVariant& b){return QString::localeAwareCompare(a.toMap()["name"].toString(),b.toMap()["name"].toString())<0;});return rows;
}
QVariantList installedApplicationLibrary(const std::atomic_bool* cancel,bool* complete){
    ComScope com;QStringList roots,registered,running;
    trace("library:start");
    for(auto id:{&FOLDERID_Programs,&FOLDERID_CommonPrograms,&FOLDERID_Desktop,&FOLDERID_PublicDesktop}){
        PWSTR path=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(*id,0,nullptr,&path))){roots.push_back(QString::fromWCharArray(path));CoTaskMemFree(path);}
    }
    trace("library:folders");
    for(auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE})for(auto view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY}){
        HKEY key=nullptr;if(RegOpenKeyExW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths",0,KEY_READ|view,&key)!=ERROR_SUCCESS)continue;
        for(DWORD index=0;;++index){
            if(cancel&&cancel->load())break;wchar_t name[512]{};DWORD count=512;if(RegEnumKeyExW(key,index,name,&count,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;
            wchar_t value[32768]{};DWORD bytes=sizeof(value);
            if(RegGetValueW(key,name,nullptr,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,value,&bytes)==ERROR_SUCCESS){auto path=QString::fromWCharArray(value).trimmed();if(path.startsWith('"')&&path.endsWith('"'))path=path.mid(1,path.size()-2);registered.push_back(path);}
        }RegCloseKey(key);
    }
    trace("library:registry");
    if(!cancel||!cancel->load())for(auto& path:applicationPaths())running.push_back(QString::fromUtf8(path.data(),static_cast<qsizetype>(path.size())));
    trace("library:windows");
    auto rows=scanApplicationLibrary(roots,registered,running,cancel,complete);
    trace("library:scanned");
    if(!cancel||!cancel->load()){
        trace("library:processes");
        QSet<QString> active;for(auto& path:processPaths())active.insert(normalized(QString::fromUtf8(path.data(),static_cast<qsizetype>(path.size()))).toCaseFolded());
        for(auto& item:rows){auto row=item.toMap();row["running"]=active.contains(row["path"].toString().toCaseFolded());item=row;}
    }
    trace("library:done");
    return rows;
}
}
