#include "qt_bridge.hpp"
#include "app_icons.hpp"
#include "app_catalog.hpp"
#include "app_library.hpp"
#include "bridge_test_access.hpp"
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <objbase.h>
#include <shobjidl.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
using namespace muse;
static int checks=0;
static void require(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
static void settle(Bridge& b){QElapsedTimer elapsed;elapsed.start();while(b.savingSelection()&&elapsed.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(10);}require(!b.savingSelection(),"selection save finishes");}
static void drain(Bridge& b){QElapsedTimer elapsed;elapsed.start();while((b.busy()||!b.pendingAction().isEmpty()||b.savingSelection())&&elapsed.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(10);}require(!b.busy()&&b.pendingAction().isEmpty()&&!b.savingSelection(),"queued operations finish");}
static void makeShortcut(const fs::path& linkPath,const fs::path& target){
    auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ComPtr<IShellLinkW> link;require(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))),"create test shortcut");
    link->SetPath(target.c_str());ComPtr<IPersistFile> file;require(SUCCEEDED(link.As(&file))&&SUCCEEDED(file->Save(linkPath.c_str(),TRUE)),"save test shortcut");file.Reset();link.Reset();if(SUCCEEDED(hr))CoUninitialize();
}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    auto dir=exeDir()/wide("ui-regression-"+randomToken());fs::create_directories(dir);
    try{
        if(argc==3&&std::string(argv[1])=="--inspect-subscription"){
            fs::copy_file(fs::path(wide(argv[2])),dir/L"settings.dat");
            int inspectionResult=1;
            {
            Bridge b(dir,false);b.savePorts(27899,29189);b.refreshProcesses();b.loadNodes();QElapsedTimer elapsed;elapsed.start();
            while((b.busy()||b.catalogBusy())&&elapsed.elapsed()<60000){app.processEvents();QThread::msleep(30);}
            int minimum=65536,chosen=-1;
            for(auto& row:b.nodeDetails()){auto n=row.toMap();int d=n["delay"].toInt();if(d>0)minimum=std::min(minimum,d);if(n["name"].toString()==b.currentNode())chosen=d;}
            bool startupOptimized=minimum<65536&&chosen==minimum;
            bool selectionOk=false;
            bool manualPreserved=false;
            if(b.loaded()&&!b.nodeDetails().empty()){
                auto target=b.nodeDetails().back().toMap()["name"].toString();b.selectNode(target);elapsed.restart();
                while(b.busy()&&elapsed.elapsed()<15000){app.processEvents();QThread::msleep(30);}selectionOk=b.currentNode()==target;
                b.refreshNodes();elapsed.restart();while(b.busy()&&elapsed.elapsed()<15000){app.processEvents();QThread::msleep(30);}manualPreserved=b.currentNode()==target;
            }
            int quota=0;for(auto& row:b.subscriptions())if(row.toMap()["usageKnown"].toBool())++quota;
            int measured=0,responsive=0;for(auto& row:b.nodeDetails()){auto delay=row.toMap()["delay"].toInt();measured+=delay>=0;responsive+=delay>0;}
            int primaryApps=0,unstarted=0;for(auto& row:b.processes()){primaryApps+=!row.toMap()["background"].toBool();unstarted+=!row.toMap()["running"].toBool();}
            Json report={{"loaded",b.loaded()},{"proxy_active",b.connected()},{"node_count",b.nodeDetails().size()},{"library_apps",primaryApps},{"unstarted_apps",unstarted},{"subscriptions_with_quota",quota},{"measured_nodes",measured},{"responsive_nodes",responsive},{"startup_optimized",startupOptimized},{"minimum_ms",minimum},{"selected_ms",chosen},{"node_switch_ok",selectionOk},{"manual_selection_preserved",manualPreserved},{"error",b.error()}};
            writeFile(exeDir()/L"subscription-check.json",report.dump(2));
            std::cout<<report.dump()<<'\n';
            if(!b.loaded())std::cerr<<b.message().toStdString()<<'\n';
            inspectionResult=b.loaded()&&!b.connected()&&selectionOk&&measured>0&&startupOptimized&&manualPreserved?0:1;
            }
            if(fs::weakly_canonical(dir).parent_path()!=fs::weakly_canonical(exeDir()))throw std::runtime_error("Unexpected inspection directory");
            fs::remove_all(dir);
            return inspectionResult;
        }
        {
            auto exe=fs::path(QCoreApplication::applicationFilePath().toStdWString());
            makeShortcut(dir/L"Offline App.lnk",exe);
            auto resolved=shortcutExecutable(QString::fromStdWString((dir/L"Offline App.lnk").wstring()));
            require(resolved.compare(QDir::toNativeSeparators(QString::fromStdWString(exe.wstring())),Qt::CaseInsensitive)==0,"shortcut resolves without starting application");
            auto offline=scanApplicationLibrary({QString::fromStdWString(dir.wstring())},{},{});
            require(offline.size()==1&&offline[0].toMap()["installed"].toBool()&&!offline[0].toMap()["running"].toBool(),"unstarted start-menu application appears in library");
            auto duplicate=scanApplicationLibrary({QString::fromStdWString(dir.wstring())},{resolved},{resolved});
            require(duplicate.size()==1&&duplicate[0].toMap()["running"].toBool(),"installed and running sources merge by executable path");
            require(backgroundApplication("C:/Windows/System32/svchost.exe"),"system services filtered");
            require(backgroundApplication("D:/Apps/WeChat/crashpad_handler.exe"),"crash reporters filtered");
            require(backgroundApplication("D:/Apps/Codex/codex-code-mode-host.exe"),"background code helpers filtered");
            require(!backgroundApplication("D:/Apps/Codex/ChatGPT.exe"),"primary Codex application retained");
            require(!backgroundApplication("C:/Windows/System32/notepad.exe"),"user-facing Windows applications are not services");
            require(applicationCategory("D:/Browser/msedge.exe","Edge")=="浏览器","browser classification");
            require(applicationCategory("D:/SDK/DXViewer.exe","DirectX Sample Browser")=="工作开发","SDK sample browser is a development tool");
            require(applicationCategory("D:/Chat/Weixin.exe","微信")=="通讯社交","social classification");
            require(applicationCategory("D:/Game/Steam.exe","Steam")=="游戏影音","game classification");
            require(applicationCategory("D:/Apps/ChatGPT.exe","Codex")=="工作开发","work application classification");
            require(nodeRegion("JP 东京 01 🇯🇵")=="日本"&&nodeRegion("HK-02 香港")=="香港","region classification");
            require(nodeRegion("Australia AU-01")=="澳洲","US does not match Australia");
            require(nodeRegion("Custom relay")=="其他地区","unknown nodes remain selectable");
            Bridge b(dir,false);
            require(b.selectedPaths().empty()&&b.mode()==1,"empty selection defaults to global TUN");
            for(auto& row:b.processes())require(!row.toMap()["background"].toBool(),"default application catalog has no background helpers");
            int resets=0;QObject::connect(&b,&Bridge::processesChanged,[&]{++resets;});
            std::promise<void> release;BridgeTestAccess::holdBackground(b,release.get_future().share());b.selectNode("unused");b.refreshNodes();require(!b.pendingAction().isEmpty(),"click during background work is acknowledged instead of dropped");release.set_value();
            QElapsedTimer queueWait;queueWait.start();while((b.busy()||!b.pendingAction().isEmpty())&&queueWait.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(10);}require(!b.busy()&&b.pendingAction().isEmpty(),"queued operations drain in order");
            auto executable=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            b.toggleProcess(executable);require(b.selectedPaths().size()==1&&b.mode()==2,"select one automatically changes to process routing");
            require(resets==0,"selection does not reset application grid or scroll");
            b.toggleProcess("D:/Apps/Foo, Inc/test.exe");require(b.error()&&b.selectedPaths().size()==1,"invalid routing paths cannot enter selections");
            auto second=QDir::toNativeSeparators(QCoreApplication::applicationDirPath()+"/another.exe");
            b.toggleProcess(second);require(b.selectedPaths().size()==2,"multiple selection");
            b.toggleProcess(executable.toUpper());require(b.selectedPaths().size()==1,"Windows paths compare case insensitively");
            b.toggleProcess(second);require(b.selectedPaths().empty()&&b.mode()==1,"deselect last restores global");
            b.toggleProcess(executable);b.clearProcesses();require(b.selectedPaths().empty(),"restore global clears selection");
            settle(b);auto port=b.mixedPort();b.savePorts(0,0);require(b.error()&&b.mixedPort()==port,"invalid ports cannot mutate saved settings");
            b.saveSubscription(-1,"Test","http://invalid",3600);require(b.error()&&b.subscriptions().empty(),"invalid subscription remains unsaved");
            b.saveSubscription(-1,"Test","https://example.com/sub",3600);require(b.subscriptions().size()==1&&!b.loaded()&&!b.connected(),"saving alone does not change machine proxy or fetch network");
            fs::rename(dir/L"settings.dat",dir/L"settings.saved");fs::create_directory(dir/L"settings.dat");
            b.toggleProcess(executable);b.connectOrDisconnect();require(!b.pendingAction().isEmpty(),"start request acknowledges pending selection save");settle(b);require(b.error()&&b.selectedPaths().empty()&&b.mode()==1,"failed background save rolls back selection and effective route");require(b.pendingAction().isEmpty()&&!b.connected()&&!b.busy(),"save failure cancels queued start instead of enabling wrong routing scope");
            fs::remove(dir/L"settings.dat");fs::rename(dir/L"settings.saved",dir/L"settings.dat");
            AppIcons icons;QSize size;auto image=icons.requestImage(QString::fromLatin1(executable.toUtf8().toHex()),&size,{64,64});
            require(!image.isNull()&&size.isValid(),"real executable icon extracted");
            auto fallback=icons.requestImage(QString::fromLatin1(second.toUtf8().toHex()),&size,{64,64});require(!fallback.isNull(),"missing application has fallback icon");
        }
        {Bridge reloaded(dir,false);require(reloaded.selectedPaths().empty()&&reloaded.mode()==1&&reloaded.subscriptions().size()==1,"settings reload preserves automatic mode and subscriptions");}
        {
            Bridge b(dir,false);
            b.saveSubscription(-1,"Second","https://example.com/second",3600);
            b.saveSubscription(-1,"Third","https://example.com/third",3600);
            std::promise<void> release;BridgeTestAccess::holdBackground(b,release.get_future().share());
            b.enableSubscription(0,false);b.enableSubscription(1,false);release.set_value();drain(b);
            require(!b.subscriptions()[0].toMap()["enabled"].toBool()&&!b.subscriptions()[1].toMap()["enabled"].toBool(),"queued changes to different subscriptions are not coalesced together");
            std::promise<void> shifted;BridgeTestAccess::holdBackground(b,shifted.get_future().share());
            b.removeSubscription(0);b.saveSubscription(1,"Edited Second","https://example.com/edited",3600);shifted.set_value();drain(b);
            require(b.subscriptions().size()==2&&b.subscriptions()[0].toMap()["name"].toString()=="Edited Second"&&b.subscriptions()[1].toMap()["name"].toString()=="Third","queued edits follow subscription identity after earlier removal");
            const auto catalogPath=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            BridgeTestAccess::publishPartialCatalog(b,{QVariantMap{{"path",catalogPath},{"name","Retained application"},{"background",false},{"category","其他应用"},{"icon",QString::fromLatin1(catalogPath.toUtf8().toHex())}}});
            auto before=b.processes();require(!before.empty(),"cached application exists for partial scan regression");
            BridgeTestAccess::publishPartialCatalog(b,{});require(b.processes().size()==before.size(),"partial scans never erase the existing library");
        }
        {
            auto emptyDir=dir/L"empty";fs::create_directory(emptyDir);Bridge b(emptyDir,false);
            BridgeTestAccess::markConnected(b);std::promise<void> release;BridgeTestAccess::holdBackground(b,release.get_future().share());
            b.connectOrDisconnect();release.set_value();drain(b);
            require(!b.connected()&&!b.error(),"queued disconnect does not turn into a start when core state changes");
            fs::remove(emptyDir);
        }
        {
            auto brokenDir=dir/L"broken";fs::create_directory(brokenDir);
            writeFile(brokenDir/L"settings.dat","unreadable settings fixture");
            {Bridge b(brokenDir,false);require(b.error()&&b.locked(),"unreadable settings prevent edits");b.toggleProcess(QCoreApplication::applicationFilePath());require(b.selectedPaths().empty()&&readFile(brokenDir/L"settings.dat")=="unreadable settings fixture","failed settings load never overwrites the original file");}
            fs::remove(brokenDir/L"settings.dat");fs::remove(brokenDir);
        }
        for(auto& file:fs::directory_iterator(dir))if(file.is_regular_file())fs::remove(file.path());fs::remove(dir);
        std::cout<<checks<<" UI and icon regression checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
