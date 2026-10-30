#include "qt_bridge.hpp"
#include "app_icons.hpp"
#include "bridge_test_access.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <iostream>
using namespace muse;
static int checks=0;
static void require(bool okay,const char* text){++checks;if(!okay)throw std::runtime_error(text);}
template<class F>static bool until(F test,int limit=3000){QElapsedTimer time;time.start();do{QCoreApplication::processEvents();if(test())return true;QThread::msleep(5);}while(time.elapsed()<limit);return false;}
static void click(QQuickWindow* window,QQuickItem* item,QPointF point={}){
    require(item!=nullptr,"click target exists");if(point.isNull())point={item->width()/2,item->height()/2};auto scene=item->mapToScene(point);auto global=window->mapToGlobal(scene.toPoint());
    QMouseEvent move(QEvent::MouseMove,scene,global,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&move);
    QMouseEvent down(QEvent::MouseButtonPress,scene,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&down);
    QMouseEvent up(QEvent::MouseButtonRelease,scene,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&up);QCoreApplication::processEvents();
}
static QQuickItem* visualItem(QQuickItem* root,const QString& name){if(root->objectName()==name)return root;for(auto* child:root->childItems())if(auto* result=visualItem(child,name))return result;return nullptr;}
int main(int argc,char** argv){
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);QQuickStyle::setStyle("Basic");QGuiApplication app(argc,argv);
    auto dir=exeDir()/wide("qml-regression-"+randomToken());fs::create_directories(dir);
    try{
        auto source=fs::path(QCoreApplication::applicationFilePath().toStdWString());auto first=dir/L"Alpha.exe",second=dir/L"Beta.exe";fs::copy_file(source,first);fs::copy_file(source,second);
        auto pathA=QString::fromStdWString(first.wstring()),pathB=QString::fromStdWString(second.wstring());
        QVariantList rows;
        for(auto path:{pathA,pathB})rows.push_back(QVariantMap{{"path",path},{"name",path==pathA?"Alpha Offline":"Beta Offline"},{"filename",path==pathA?"Alpha.exe":"Beta.exe"},{"category","工作开发"},{"installed",true},{"running",false},{"background",false},{"icon",QString::fromLatin1(path.toUtf8().toHex())}});
        Settings settings;settings.libraryPaths={pathA.toStdString()};saveProtected(dir/L"settings.dat",serialize(settings).dump());saveProtected(dir/L"application-cache.dat",QJsonDocument::fromVariant(rows).toJson().toStdString());
        {
            Bridge bridge(dir,false);QQmlApplicationEngine engine;engine.addImageProvider("apps",new AppIcons);engine.rootContext()->setContextProperty("backend",&bridge);engine.load(QUrl("qrc:/qml/Main.qml"));
            require(!engine.rootObjects().empty(),"all QML components load without errors");auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());require(window!=nullptr,"application window exists");
            auto item=[&](const QString& name){return visualItem(window->contentItem(),name);};
            require(until([&]{auto* nav=item("nav-2");return nav&&nav->width()>0;}),"navigation buttons materialize in visual tree");
            std::promise<void> backgroundDone;BridgeTestAccess::holdBackground(bridge,backgroundDone.get_future().share());
            click(window,item("nav-2"));require(until([&]{return window->property("page").toInt()==2;}),"one click switches page");
            require(until([&]{return item("source-2")!=nullptr;}),"application source filters materialize");click(window,item("source-2"));
            require(until([&]{auto* grid=item("application-grid");return grid&&grid->property("count").toInt()==2;}),"unstarted application filter includes both installed applications");
            auto name="application-"+QString::fromLatin1(pathA.toUtf8().toHex());require(until([&]{return item(name)!=nullptr;}),"offline application card materializes");
            BridgeTestAccess::rebuildCatalog(bridge);
            require(until([&]{auto* grid=item("application-grid");return std::abs(grid->property("contentY").toDouble()-grid->property("originY").toDouble())<1;}),"catalog refresh preserves the beginning without clipping first row");
            QElapsedTimer clickTime;clickTime.start();auto* card=item(name);click(window,card,{card->width()-23,24});
            require(until([&]{return bridge.selectedPaths().size()==1;},500),"checkbox responds to first click");require(clickTime.elapsed()<500,"selection feedback is prompt");
            card=item(name);click(window,card,{card->width()-23,24});require(until([&]{return bridge.selectedPaths().empty();}),"second click deselects");
            require(until([&]{return !bridge.savingSelection();}),"selection writes finish in background");
            require(bridge.busy(),"clicks and saves succeed while a background request is still pending");backgroundDone.set_value();require(until([&]{return !bridge.busy();}),"background request completes independently");
            click(window,item("source-3"));require(until([&]{return item("application-grid")->property("count").toInt()==1;}),"manual application remains after deselection");
            for(auto size:{QSize(980,690),QSize(1120,760),QSize(1440,900)}){
                window->resize(size);require(until([&]{auto* grid=item("application-grid");return grid->height()>180&&grid->mapToScene({0,grid->height()}).y()<=window->height()-35;}),"application grid remains usable at supported window sizes");
                auto* grid=item("application-grid");auto bottom=grid->mapToScene({0,grid->height()}).y();require(bottom<=window->height()-35,"application grid does not overlap footer");
                click(window,item("nav-3"));require(until([&]{return window->property("page").toInt()==3;}),"preferences navigation responds");auto* scroll=item("settings-scroll");require(scroll&&scroll->height()>200,"preferences stay scrollable at minimum height");auto* tun=item("tun-switch");require(tun!=nullptr,"TUN switch materializes");require(tun->property("checked").toBool()==bridge.tunEnabled(),"TUN switch reflects the stored setting");require(tun->isEnabled(),"TUN switch stays editable while idle");
                click(window,item("nav-2"));require(until([&]{return window->property("page").toInt()==2;}),"return navigation responds");
            }
            click(window,item("nav-3"));
            fs::rename(dir/L"settings.dat",dir/L"settings.saved");fs::create_directory(dir/L"settings.dat");
            const bool tunBefore=bridge.tunEnabled();click(window,item("tun-switch"));
            require(bridge.error()&&bridge.tunEnabled()==tunBefore&&item("tun-switch")->property("checked").toBool()==tunBefore,"failed TUN save restores the switch to the actual stored value");
            fs::remove(dir/L"settings.dat");fs::rename(dir/L"settings.saved",dir/L"settings.dat");
            click(window,item("nav-0"));require(until([&]{return window->property("page").toInt()==0;}),"overview navigation responds");
            bool quit=false;QObject::connect(&bridge,&Bridge::readyToQuit,[&]{quit=true;});bridge.requestQuit();require(until([&]{return quit;}),"quit request is acknowledged and completes safely");
        }
        require(fs::weakly_canonical(dir).parent_path()==fs::weakly_canonical(exeDir()),"cleanup stays within isolated test directory");fs::remove_all(dir);
        std::cout<<checks<<" QML click, offline-selection and layout checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
