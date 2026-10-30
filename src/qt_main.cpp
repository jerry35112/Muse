#include "qt_bridge.hpp"
#include "app_icons.hpp"
#include "app_library.hpp"

#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTimer>

#include <dwmapi.h>

#include <memory>

namespace {

#ifndef MUSE_VERSION
#define MUSE_VERSION "0.5.1"
#endif

constexpr int kMaxPage = 4;
constexpr int kCatalogCaptureLimitMs = 15000;

struct LaunchOptions {
    int page = -1;
    QSize size;
    QString screenshot;
    QString dumpCatalog;
    int captureDelayMs = 1500;
    bool smokeTest = false;
    bool previewSubscription = false;
};

// Accepts both "--flag value" and "--flag=value": scripts and shortcuts use both.
QString inlineValue(const QString& argument, const QString& flag) {
    return argument.startsWith(flag + "=") ? argument.mid(flag.size() + 1) : QString{};
}

LaunchOptions parseArguments(const QStringList& args) {
    LaunchOptions options;
    for (int index = 0; index < args.size(); ++index) {
        const auto argument = args.at(index);
        const auto following = [&] { return index + 1 < args.size() ? args.at(index + 1) : QString{}; };
        const auto valueOf = [&](const QString& flag) {
            auto value = inlineValue(argument, flag);
            if (value.isEmpty() && argument == flag) value = following();
            return value;
        };

        if (argument == "--smoke-test") {
            options.smokeTest = true;
            continue;
        }
        if (argument == "--preview-subscription") {
            options.previewSubscription = true;
            continue;
        }
        if (const auto page = valueOf("--page"); !page.isEmpty()) {
            const auto number = page.toInt();
            if (number >= 0 && number <= kMaxPage) options.page = number;
            continue;
        }
        if (const auto size = valueOf("--size"); !size.isEmpty()) {
            const auto match = QRegularExpression("^(\\d+)x(\\d+)$").match(size);
            if (match.hasMatch()) options.size = QSize(match.captured(1).toInt(), match.captured(2).toInt());
            continue;
        }
        if (const auto shot = valueOf("--screenshot"); !shot.isEmpty()) {
            options.screenshot = shot;
            continue;
        }
        if (const auto delay = valueOf("--capture-delay"); !delay.isEmpty()) {
            options.captureDelayMs = qMax(0, delay.toInt());
            continue;
        }
        if (const auto dump = valueOf("--dump-catalog"); !dump.isEmpty()) options.dumpCatalog = dump;
    }
    return options;
}

// A second launch should surface the running window instead of exiting silently.
// Only async calls: the other instance may be busy or hung, and blocking on it
// would make the second launch hang too.
void activateExistingInstance() {
    const auto window = FindWindowW(nullptr, L"Muse");
    if (!window) return;
    ShowWindowAsync(window, IsIconic(window) ? SW_RESTORE : SW_SHOW);
}

void applyWindowChrome(QQuickWindow* window) {
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    // Frameless window: keep a 1 px DWM frame so resizing, snapping and shadows work.
    MARGINS margins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2 (Windows 11).
    DWORD corner = 2;
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
}

void startCapture(QQuickWindow* window, muse::Bridge& bridge, const QString& path, int delayMs) {
    auto* timer = new QTimer(window);
    timer->setInterval(100);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    QObject::connect(timer, &QTimer::timeout, window, [timer, elapsed, window, &bridge, path, delayMs] {
        // Wait for the first catalog scan so screenshots show real applications.
        if (elapsed->elapsed() < delayMs) return;
        if (bridge.catalogBusy() && elapsed->elapsed() < kCatalogCaptureLimitMs) return;
        timer->stop();
        const bool saved = window->grabWindow().save(path);
        QCoreApplication::exit(saved ? 0 : 1);
    });
    timer->start();
}

} // namespace

int main(int argc, char** argv) {
    muse::trace("main:enter");
    HANDLE singleton = CreateMutexW(nullptr, TRUE, L"Local\\Muse.Desktop.SingleInstance");
    const bool alreadyRunning = singleton && GetLastError() == ERROR_ALREADY_EXISTS;
    if (alreadyRunning || !singleton) {
        muse::trace(alreadyRunning ? "main:second-instance" : "main:mutex-unavailable");
        if (singleton) CloseHandle(singleton);
        if (alreadyRunning) activateExistingInstance();
        return 0;
    }

    muse::trace("main:start");
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("Muse");
    app.setApplicationVersion(QStringLiteral(MUSE_VERSION));
    app.setWindowIcon(QIcon(":/assets/muse.png"));
    QFont font("Microsoft YaHei UI");
    font.setPixelSize(13);
    app.setFont(font);

    muse::trace("main:app");
    const auto options = parseArguments(app.arguments());
    // Diagnostic: dump the discovered application library and exit, without GUI.
    if (!options.dumpCatalog.isEmpty()) {
        try {
            const auto rows = muse::installedApplicationLibrary(nullptr);
            auto json = QJsonDocument::fromVariant(rows).toJson(QJsonDocument::Indented);
            muse::writeFile(options.dumpCatalog.toStdWString(), std::string(json.constData(), json.size()));
            CloseHandle(singleton);
            return 0;
        } catch (const std::exception& e) {
            qCritical("%s", e.what());
            CloseHandle(singleton);
            return 1;
        }
    }
    int result = 1;
    try {
        muse::trace("main:bridge-before");
        muse::Bridge bridge;
        muse::trace("main:bridge-after");
        QQmlApplicationEngine engine;
        // Terminating is owned by Main.qml: the window only accepts a close once
        // the shutdown sequence finished, so quit() is routed through that path.
        engine.addImageProvider("apps", new muse::AppIcons);
        engine.rootContext()->setContextProperty("backend", &bridge);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        muse::trace("main:qml-loaded");
        if (engine.rootObjects().isEmpty()) {
            qCritical("无法加载界面 qrc:/qml/Main.qml");
            CloseHandle(singleton);
            return 1;
        }

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window) {
            qCritical("界面根对象不是窗口");
            CloseHandle(singleton);
            return 1;
        }

        if (options.page >= 0) window->setProperty("page", options.page);
        if (options.size.isValid()) window->resize(options.size);
        if (options.previewSubscription) QMetaObject::invokeMethod(window, "previewSubscriptionEditor");
        bridge.setWindow(reinterpret_cast<HWND>(window->winId()));
        applyWindowChrome(window);
        if (options.smokeTest) QTimer::singleShot(1200, &app, &QCoreApplication::quit);
        if (!options.screenshot.isEmpty()) {
            window->setProperty("captureMode", true);
            startCapture(window, bridge, options.screenshot, options.captureDelayMs);
        }
        muse::trace("main:exec");
        result = app.exec();
        muse::trace("main:exec-returned");
    } catch (const std::exception& e) {
        qCritical("%s", e.what());
    }
    muse::trace("main:exit");
    CloseHandle(singleton);
    return result;
}
