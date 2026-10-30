#include "app_catalog.hpp"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
namespace muse {
bool backgroundApplication(const QString& path){
    auto normalized=QDir::fromNativeSeparators(path).toLower();auto file=QFileInfo(normalized).fileName();
    static const QStringList helpers={"crashpad_handler.exe","applicationframehost.exe","runtimebroker.exe","ctfmon.exe","crossdeviceresume.exe","dllhost.exe","conhost.exe","svchost.exe","sihost.exe","searchhost.exe","startmenuexperiencehost.exe","shellexperiencehost.exe","textinputhost.exe","nhnotifysys.exe","cefviewwing.exe","msedgewebview2.exe","codex.exe","codex-code-mode-host.exe","codex-computer-use.exe","codex-computer-use-swift.exe","extension-host.exe","node.exe","python.exe","pythonw.exe","mihomo.exe","muse.exe","updater.exe","update.exe","notification_helper.exe"};
    return helpers.contains(file)||file.endsWith("crashreporter.exe")||file.endsWith("_helper.exe");
}
QString applicationCategory(const QString& path,const QString& name){
    auto file=QFileInfo(QDir::fromNativeSeparators(path)).completeBaseName().toLower();
    auto value=(file+" "+name).toLower();
    auto any=[&](std::initializer_list<const char*> terms){for(auto term:terms)if(value.contains(QString::fromUtf8(term)))return true;return false;};
    if(any({"directx","cmake","dbgx","sample browser","texture tool","error lookup","sdk"}))return "工作开发";
    if(any({"msedge","chrome","firefox","brave","opera","vivaldi","librewolf","floorp","browser","浏览器"})||file=="zen"||file=="arc")return "浏览器";
    if(any({"wechat","weixin","telegram","discord","slack","teams","signal","skype","whatsapp","wxwork","dingtalk","feishu","lark","zoom","微信","钉钉","飞书","闲鱼"})||file=="qq"||file=="tim")return "通讯社交";
    if(any({"steam","epicgames","battlenet","wegame","mumu","nox","ldplayer","dnplayer","spotify","potplayer","vlc","music","player","游戏","音乐","模拟器"}))return "游戏影音";
    if(any({"clash","warp","v2ray","sing-box","nekoray","hiddify","proxy","wireshark","tailscale"}))return "网络工具";
    if(any({"chatgpt","codex","deepseek","doubao","code","devenv","rider","webstorm","pycharm","idea","notepad","obsidian","winword","excel","powerpnt","wps","postman","terminal","cursor","sublime","typora","豆包"}))return "工作开发";
    return "其他应用";
}
QString nodeRegion(const QString& name){
    auto upper=name.toUpper();
    auto match=[&](QString code,QStringList words){for(auto& word:words)if(upper.contains(word.toUpper()))return true;return QRegularExpression("(?:^|[^A-Z])"+code+"(?:$|[^A-Z])").match(upper).hasMatch();};
    if(match("HK",{"香港","HONG KONG","HONGKONG","🇭🇰"}))return "香港";
    if(match("TW",{"台湾","台灣","TAIWAN","🇹🇼"}))return "台湾";
    if(match("JP",{"日本","东京","大阪","JAPAN","TOKYO","🇯🇵"}))return "日本";
    if(match("SG",{"新加坡","狮城","SINGAPORE","🇸🇬"}))return "新加坡";
    if(match("KR",{"韩国","韓國","首尔","KOREA","🇰🇷"}))return "韩国";
    if(match("US",{"美国","美國","洛杉矶","硅谷","西雅图","UNITED STATES","LOS ANGELES","🇺🇸"}))return "美国";
    if(match("(UK|GB|DE|FR|NL|IE|CH|SE|FI)",{"英国","德国","法国","荷兰","瑞士","爱尔兰","瑞典","芬兰","LONDON","GERMANY","FRANCE","🇬🇧","🇩🇪","🇫🇷","🇳🇱","🇨🇭","🇮🇪","🇸🇪","🇫🇮"}))return "欧洲";
    if(match("(AU|NZ)",{"澳大利亚","澳洲","悉尼","新西兰","AUSTRALIA","🇦🇺","🇳🇿"}))return "澳洲";
    return "其他地区";
}
}
