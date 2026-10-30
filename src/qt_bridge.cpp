#include "qt_bridge.hpp"

#include "app_catalog.hpp"
#include "app_icons.hpp"
#include "app_library.hpp"
#include "metadata.hpp"

#include <QByteArray>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUrl>

#include <shellapi.h>

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace muse {
namespace {

QString qs(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

// Selection clicks are coalesced for a moment, then flushed as one encrypted write.
constexpr int kSelectionSaveDelayMs = 180;
// Housekeeping cadence: drain futures, run the queued actions, keep the UI honest.
constexpr int kPollIntervalMs = 200;
// Node state is re-read from the core roughly every 10 s while the core is alive.
constexpr int kSyncTicks = 50;
constexpr int kDelayProbeTimeoutMs = 30000;
constexpr int kProviderUpdateTimeoutMs = 20000;

QString providerDelayProbe() {
    return QStringLiteral("/group/PROXY/delay?url=https%3A%2F%2Fwww.gstatic.com%2Fgenerate_204&timeout=5000");
}

QString bytesText(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    while (bytes >= 1024 && unit < 4) { bytes /= 1024; ++unit; }
    return QString::number(bytes, 'f', unit ? 2 : 0) + " " + units[unit];
}

QString delayText(int delay) {
    return delay > 0 ? QString::number(delay) + " ms" : delay == 0 ? QStringLiteral("超时") : QStringLiteral("未测速");
}

QString checkedProgramPath(QString path) {
    path = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (path.size() < 4 || !path[0].isLetter() || path[1] != ':' ||
        (path[2] != '\\' && path[2] != '/') || !QFileInfo(path).isAbsolute() ||
        !path.endsWith(".exe", Qt::CaseInsensitive) || path.contains(',') ||
        path.contains('\n') || path.contains('\r') || path.contains(QChar::Null))
        throw std::runtime_error("请选择本机完整路径的 .exe 程序，路径不能包含逗号或换行");
    return path;
}

} // namespace

// Shared between the GUI thread and the detached scan thread. The scan may block
// inside a shell or COM call, so it owns no Bridge state and is never joined.
struct Bridge::CatalogJob {
    std::mutex mutex;
    std::shared_ptr<std::atomic_bool> cancel = std::make_shared<std::atomic_bool>(false);
    bool running = true;
    bool quickReady = false;
    bool complete = false;
    QString error;
    QVariantList quick;
    QVariantList rows;
};

Bridge::Bridge(QObject* parent) : Bridge(dataDir(), true, parent) {}

Bridge::Bridge(fs::path directory, bool recover, QObject* parent)
    : QObject(parent), dir_(std::move(directory)), engine_(dir_), autoManage_(recover) {
    if (recover) guard([&] { engine_.recover(); });
    try {
        if (fs::exists(dir_ / L"settings.dat")) settings_ = deserialize(Json::parse(loadProtected(dir_ / L"settings.dat")));
    } catch (const std::exception&) {
        settingsReadFailed_ = true;
        reject("设置文件无法读取，已阻止覆盖。请先备份并修复 data/settings.dat，再重新启动 Muse。");
    }

    // The catalog cache lets the application grid paint before the first scan ends.
    try {
        if (fs::exists(dir_ / L"application-cache.dat")) {
            baseCatalog_ = QJsonDocument::fromJson(QByteArray::fromStdString(loadProtected(dir_ / L"application-cache.dat"))).array().toVariantList();
            for (auto& item : baseCatalog_) {
                auto row = item.toMap();
                row["running"] = false;
                item = row;
            }
        }
    } catch (...) {
    }

    rebuildProcesses();
    selectionCheckpoint_ = settings_;
    selectionDebounce_.setSingleShot(true);
    selectionDebounce_.setInterval(kSelectionSaveDelayMs);
    connect(&selectionDebounce_, &QTimer::timeout, this, &Bridge::beginSelectionSave);
    connect(&timer_, &QTimer::timeout, this, &Bridge::poll);
    timer_.start(kPollIntervalMs);

    if (recover) QTimer::singleShot(0, this, &Bridge::refreshProcesses);
    const bool hasEnabledSubscription = std::any_of(settings_.subscriptions.begin(), settings_.subscriptions.end(),
                                                    [](const Subscription& sub) { return sub.enabled; });
    if (recover && hasEnabledSubscription) QTimer::singleShot(0, this, &Bridge::loadNodes);
}

Bridge::~Bridge() {
    if (catalogJob_) catalogJob_->cancel->store(true);
    cancelWork_.store(true);
    // Workers capture this object. They must finish before its members are destroyed.
    if (task_.valid()) task_.wait();
    try { flushSelection(); } catch (...) {}
}

// ---------------------------------------------------------------- feedback ---

void Bridge::notify(QString message) {
    message_ = std::move(message);
    error_ = false;
    emit changed();
}

void Bridge::reject(QString message, QString queue) {
    if (!queue.isEmpty()) {
        message_ = message + "已加入队列：" + queue;
        error_ = false;
    } else {
        message_ = std::move(message);
        error_ = true;
    }
    emit changed();
}

QString Bridge::version() const {
#ifdef MUSE_VERSION
    return QStringLiteral(MUSE_VERSION);
#else
    return QCoreApplication::applicationVersion();
#endif
}

QString Bridge::lockReason() const {
    if (settingsReadFailed_) return QStringLiteral("设置文件读取失败，已阻止覆盖原文件");
    if (closing_) return QStringLiteral("正在退出，稍后再试");
    if (connected_) return QStringLiteral("代理运行中，先关闭代理再修改");
    if (transitioning_) return QStringLiteral("正在切换代理状态…");
    // Quiet work (delay probing, provider refresh) never blocks editing.
    if (busy_ && !syncing_) return QStringLiteral("正在应用设置…");
    return {};
}

// Returns true when the caller may run its change immediately. Otherwise it has
// already told the user what happened: refused with a reason, or queued.
bool Bridge::canEditSettings(const QString& action, std::function<void()> retry, QString key) {
    if (closing_) {
        reject("正在退出，" + action + "已取消。");
        return false;
    }
    if (connected_) {
        reject(action + "需要先关闭代理。");
        return false;
    }
    if (busy_ || savingSelection()) {
        if (retry) {
            defer(action, std::move(retry), std::move(key));
        } else {
            reject(action + "：当前任务还没结束，请稍后再试。");
        }
        return false;
    }
    return true;
}

// ------------------------------------------------------------- persistence ---

void Bridge::guard(std::function<void()> fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        error_ = true;
        message_ = qs(e.what());
    }
    emit changed();
}

void Bridge::commit(Settings settings) {
    if (settingsReadFailed_) throw std::runtime_error("原设置无法读取，不能覆盖保存");
    flushSelection();
    saveProtected(dir_ / L"settings.dat", serialize(settings).dump());
    settings_ = std::move(settings);
    selectionCheckpoint_ = settings_;
    selectionSaveError_ = false;
}

void Bridge::commitSelection(Settings settings) {
    if (settingsReadFailed_) throw std::runtime_error("原设置无法读取，不能覆盖保存");
    settings_ = std::move(settings);
    selectionSaveError_ = false;
    selectionDirty_ = true;
    selectionDebounce_.start();
    emit selectionStateChanged();
}

void Bridge::beginSelectionSave() {
    if (selectionSave_.valid() || !selectionDirty_) return;
    writtenSelection_ = settings_;
    const auto snapshot = writtenSelection_;
    const auto path = dir_ / L"settings.dat";
    try {
        selectionSave_ = std::async(std::launch::async, [path, snapshot] { saveProtected(path, serialize(snapshot).dump()); });
        selectionDirty_ = false;
    } catch (const std::exception& e) {
        rollbackSelection();
        message_ = qs(e.what());
        error_ = true;
        emit changed();
    }
    emit selectionStateChanged();
}

void Bridge::finishSelectionSave() {
    try {
        selectionSave_.get();
        selectionCheckpoint_ = writtenSelection_;
        selectionSaveError_ = false;
    } catch (...) {
        rollbackSelection();
        throw;
    }
    emit selectionStateChanged();
}

void Bridge::flushSelection() {
    selectionDebounce_.stop();
    if (selectionSave_.valid()) finishSelectionSave();
    if (selectionDirty_) {
        try {
            saveProtected(dir_ / L"settings.dat", serialize(settings_).dump());
            selectionCheckpoint_ = settings_;
            selectionDirty_ = false;
            selectionSaveError_ = false;
        } catch (...) {
            rollbackSelection();
            throw;
        }
    }
    emit selectionStateChanged();
}

void Bridge::rollbackSelection() {
    settings_ = selectionCheckpoint_;
    selectionDirty_ = false;
    selectionSaveError_ = true;
    selectionDebounce_.stop();
    // A start that was waiting on this selection must not run with the old scope.
    std::erase_if(pending_, [](const Pending& item) { return item.label == "切换代理状态"; });
    pendingAction_ = pending_.empty() ? QString{} : pending_.front().label;
    emit selectionChanged();
    emit selectionStateChanged();
    emit changed();
}

// ------------------------------------------------------------------ tasks ---

void Bridge::work(std::function<QtResult()> fn, bool quiet) {
    if (busy_) return;
    guard([&] {
        ++operationGeneration_;
        try {
            task_ = std::async(std::launch::async, std::move(fn));
        } catch (...) {
            transitioning_ = false;
            testing_ = false;
            throw;
        }
        busy_ = true;
        syncing_ = quiet;
        if (!quiet) {
            error_ = false;
            message_ = QStringLiteral("正在处理…");
        }
    });
}

void Bridge::defer(QString label, std::function<void()> action, QString key) {
    if (key.isEmpty()) key = label;
    auto existing = std::find_if(pending_.begin(), pending_.end(), [&](const Pending& item) { return item.key == key; });
    if (existing != pending_.end()) existing->action = std::move(action);
    else pending_.push_back({label, std::move(action), std::move(key)});
    pendingAction_ = pending_.front().label;
    message_ = "已收到操作：" + label + "，当前任务结束后执行。";
    error_ = false;
    emit changed();
}

void Bridge::poll() {
    pollSelectionSave();
    pollCatalogTask();
    pollWorkTask();
    if (pollShutdown()) return;
    pollQueue();
    pollEngineHealth();
    pollPeriodicRefresh();
}

void Bridge::pollSelectionSave() {
    if (!selectionSave_.valid() || selectionSave_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    guard([&] {
        finishSelectionSave();
        if (selectionDirty_) beginSelectionSave();
    });
}

void Bridge::pollCatalogTask() {
    if (!catalogBusy_ || !catalogJob_) return;
    QVariantList quick, rows;
    bool quickReady = false, done = false, complete = false;
    QString scanError;
    {
        std::lock_guard<std::mutex> lock(catalogJob_->mutex);
        if (catalogJob_->quickReady) {
            quick = catalogJob_->quick;
            catalogJob_->quickReady = false;
            quickReady = true;
        }
        if (!catalogJob_->running) {
            rows = catalogJob_->rows;
            complete = catalogJob_->complete;
            scanError = catalogJob_->error;
            done = true;
        }
    }
    // Show the running-window scan immediately, but never both in one tick: two
    // full model resets in a row cancel in-flight icon loads and leave some cards
    // without an icon.
    if (done) {
        catalogBusy_ = false;
        if (!scanError.isEmpty()) reject("应用库刷新未完成：" + scanError + "。已保留原应用库。");
        else applyCatalogResult(std::move(rows), complete);
        emit catalogStateChanged();
        return;
    }
    if (quickReady && baseCatalog_.empty() && !quick.isEmpty()) {
        baseCatalog_ = std::move(quick);
        rebuildProcesses();
    }
}

void Bridge::pollWorkTask() {
    if (!busy_ || task_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    busy_ = false;
    testing_ = false;
    syncing_ = false;
    transitioning_ = false;
    const auto previousNodes = nodeDetails();
    const auto previousSubscriptions = subscriptions();
    guard([&] {
        auto result = task_.get();
        connected_ = result.connected;
        group_ = result.group.is_object() ? result.group : Json::object();
        if (!result.proxies.empty()) proxies_ = result.proxies;
        if (!result.providers.empty()) providers_ = result.providers;
        if (!result.delays.empty()) delays_ = result.delays;
        if (!result.optimizationSummary.isEmpty()) optimizationSummary_ = result.optimizationSummary;
        if (!selectionSaveError_) {
            if (!result.message.isEmpty()) message_ = result.message;
            else if (error_) message_ = QStringLiteral("状态已恢复同步。");
            error_ = false;
        }
    });
    // A failed API call may leave an otherwise healthy core running.
    connected_ = engine_.proxyActive();
    loaded_ = engine_.alive();
    if (!loaded_) { group_ = Json::object(); proxies_ = Json::object(); providers_ = Json::object(); delays_ = Json::object(); }
    if (nodeDetails() != previousNodes) emit nodesChanged();
    if (subscriptions() != previousSubscriptions) emit subscriptionsChanged();
    emit changed();
}

bool Bridge::pollShutdown() {
    if (!closing_ || busy_) return false;
    if (engine_.alive()) {
        work([this] { engine_.stop(); return QtResult{}; }, true);
    } else if (!savingSelection()) {
        emit readyToQuit();
    }
    return true;
}

void Bridge::pollQueue() {
    if (busy_ || savingSelection() || pending_.empty()) return;
    auto next = std::move(pending_.front().action);
    pending_.pop_front();
    pendingAction_ = pending_.empty() ? QString{} : pending_.front().label;
    emit changed();
    next();
}

void Bridge::pollEngineHealth() {
    if (busy_ || engine_.alive() || (!connected_ && !loaded_ && !engine_.needsCleanup())) return;
    connected_ = false;
    work([this] {
        engine_.stop();
        return QtResult{"代理内核已退出，系统代理已恢复。", false, {}};
    });
}

void Bridge::pollPeriodicRefresh() {
    if (!loaded_ || busy_ || ++refreshTicks_ < kSyncTicks) return;
    refreshTicks_ = 0;
    work([this] { return snapshot({}); }, true);
}

// -------------------------------------------------------------- snapshots ---

QtResult Bridge::snapshot(QString message) {
    QtResult result{std::move(message), engine_.proxyActive(), engine_.request(L"GET", "/proxies/PROXY")};
    result.proxies = engine_.request(L"GET", "/proxies");
    result.providers = engine_.request(L"GET", "/providers/proxies");
    return result;
}

QtResult Bridge::optimizeNodes() {
    auto result = snapshot({});
    const auto generation = operationGeneration_;
    // Publish the fresh node list before the (slow) latency probe so the UI fills in early.
    QMetaObject::invokeMethod(this, [this, result, generation] {
        if (closing_ || !busy_ || !testing_ || operationGeneration_ != generation) return;
        group_ = result.group;
        proxies_ = result.proxies;
        providers_ = result.providers;
        loaded_ = true;
        emit nodesChanged();
        emit subscriptionsChanged();
        emit changed();
    }, Qt::QueuedConnection);

    bool choiceApplied = false;
    try {
        const auto measured = engine_.request(L"GET", providerDelayProbe().toStdString(), nullptr, kDelayProbeTimeoutMs);
        if (cancelWork_.load()) { result.message = "测速已结束，正在退出。"; return result; }
        if (!measured.is_object()) throw std::runtime_error("无效的测速结果");
        const auto candidates = result.group.value("all", Json::array());
        for (const auto& name : candidates) {
            const auto key = name.get<std::string>();
            const bool timed = measured.contains(key) && measured[key].is_number_integer();
            result.delays[key] = timed ? measured[key] : Json(0);
        }
        const auto best = fastestNode(candidates, measured, result.group.value("now", std::string{}));
        if (best.empty()) {
            result.message = "本轮节点全部超时，已保留原选择。可以稍后重新测速。";
            result.optimizationSummary = "本轮全部超时 · 保留原节点";
            return result;
        }
        Json choice = {{"name", best}};
        engine_.request(L"PUT", "/proxies/PROXY", &choice);
        choiceApplied = true;
        result.group["now"] = best;
        auto refreshed = snapshot("已自动优选最低延迟节点 · " + QString::number(measured[best].get<int>()) + " ms");
        refreshed.delays = result.delays;
        result = std::move(refreshed);
        result.optimizationSummary = result.message;
    } catch (const std::exception&) {
        result.message = choiceApplied ? "已切换到优选节点，但部分状态未同步；稍后会自动重试同步。"
                                       : "节点已加载，但自动测速未完成；请刷新状态或重新优选。";
        result.optimizationSummary = choiceApplied ? "已应用优选节点 · 等待状态同步" : "自动优选未完成 · 请刷新状态";
    }
    return result;
}

void Bridge::reloadPreview() {
    // Load a preview of freshly saved settings without touching the machine proxy.
    if ((!loaded_ && !autoManage_) || connected_ || busy_) return;
    const auto settings = settings_;
    clearNodeState();
    testing_ = true;
    work([this, settings] {
        engine_.stop();
        const bool anyEnabled = std::any_of(settings.subscriptions.begin(), settings.subscriptions.end(),
                                            [](const Subscription& sub) { return sub.enabled; });
        if (!anyEnabled) return QtResult{"订阅配置已保存。", false, {}};
        engine_.start(settings, false, {}, &cancelWork_);
        if (cancelWork_.load()) return QtResult{};
        return optimizeNodes();
    }, true);
}

void Bridge::clearNodeState() {
    group_ = Json::object(); proxies_ = Json::object(); providers_ = Json::object(); delays_ = Json::object();
    loaded_ = false;
    emit nodesChanged(); emit subscriptionsChanged(); emit changed();
}

// ---------------------------------------------------------------- readers ---

QVariantList Bridge::subscriptions() const {
    QVariantList out;
    for (const auto& sub : settings_.subscriptions) {
        QVariantMap row{{"name", qs(sub.name)},
                        {"host", QUrl(qs(sub.url)).host()},
                        {"enabled", sub.enabled},
                        {"interval", sub.interval},
                        {"usageKnown", false},
                        {"remaining", "—"},
                        {"usageText", !sub.enabled ? "订阅未启用" : loaded_ ? "服务商未提供流量限额信息" : "加载后读取订阅用量"},
                        {"expiry", "到期时间未提供"},
                        {"fraction", 0.0}};
        const auto key = providerKey(sub);
        if (!providers_.contains("providers") || !providers_["providers"].contains(key)) {
            out.push_back(row);
            continue;
        }
        const auto& provider = providers_["providers"][key];
        if (!provider.contains("subscriptionInfo") || !provider["subscriptionInfo"].is_object()) {
            out.push_back(row);
            continue;
        }
        const auto usage = subscriptionUsage(provider["subscriptionInfo"]);
        if (usage.known) {
            row["usageKnown"] = true;
            row["remaining"] = bytesText(usage.remaining);
            row["usageText"] = "已用 " + bytesText(usage.used) + " / " + bytesText(usage.total);
            row["fraction"] = usage.fraction;
        }
        if (usage.expires > 0) row["expiry"] = "到期 " + QDateTime::fromSecsSinceEpoch(usage.expires).toString("yyyy-MM-dd");
        out.push_back(row);
    }
    return out;
}

QStringList Bridge::selectedPaths() const {
    QStringList out;
    for (const auto& path : settings_.processes) out.push_back(qs(path));
    return out;
}

QVariantList Bridge::applicationCategories() const {
    QVariantList out;
    int total = 0;
    QMap<QString, int> counts;
    for (const auto& item : processModel_) {
        const auto row = item.toMap();
        if (row["background"].toBool()) continue;
        ++total;
        ++counts[row["category"].toString()];
    }
    out.push_back(QVariantMap{{"key", "全部应用"}, {"count", total}});
    for (const auto& key : QStringList{"浏览器", "通讯社交", "游戏影音", "工作开发", "网络工具", "其他应用"})
        if (counts.value(key)) out.push_back(QVariantMap{{"key", key}, {"count", counts[key]}});
    return out;
}

QStringList Bridge::nodeRegions() const {
    QSet<QString> present;
    for (const auto& row : nodeDetails()) present.insert(row.toMap()["region"].toString());
    QStringList out{"全部地区"};
    for (const auto& name : QStringList{"香港", "台湾", "日本", "新加坡", "韩国", "美国", "欧洲", "澳洲", "其他地区"})
        if (present.contains(name)) out.push_back(name);
    return out;
}

QStringList Bridge::nodes() const {
    QStringList out;
    if (group_.contains("all"))
        for (const auto& name : group_["all"]) out.push_back(qs(name.get<std::string>()));
    return out;
}

QString Bridge::currentNode() const { return qs(group_.value("now", std::string{})); }

QVariantList Bridge::nodeDetails() const {
    QVariantList out;
    for (const auto& name : nodes()) {
        if (name == "COMPATIBLE" || name == "DIRECT" || name == "REJECT") continue;
        QVariantMap node{{"name", name}, {"region", nodeRegion(name)}, {"type", "Proxy"}, {"delay", -1}, {"delayText", "未测速"}};
        const auto key = name.toStdString();
        int delay = -1;
        // The core's own health check is authoritative once it has run.
        if (proxies_.contains("proxies") && proxies_["proxies"].contains(key)) {
            const auto& detail = proxies_["proxies"][key];
            node["type"] = qs(detail.value("type", std::string("Proxy")));
            if (detail.contains("history") && detail["history"].is_array() && !detail["history"].empty())
                delay = latestDelay(detail["history"]);
        }
        if (delay < 0 && delays_.contains(key) && delays_[key].is_number_integer())
            delay = delays_[key].get<int>();
        node["delay"] = delay;
        node["delayText"] = delayText(delay);
        out.push_back(node);
    }
    // Reachable nodes first (fastest to slowest), then unmeasured, then timeouts.
    std::stable_sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        const auto score = [](const QVariant& row) {
            const int delay = row.toMap()["delay"].toInt();
            return delay > 0 ? delay : delay < 0 ? 65536 : 65537;
        };
        return score(a) < score(b);
    });
    return out;
}

// --------------------------------------------------------------- commands ---

void Bridge::setTun(bool enabled) {
    if (!canEditSettings("切换 TUN 接管", [this, enabled] { setTun(enabled); })) return;
    guard([&] {
        auto next = settings_;
        next.tun = enabled;
        commit(next);
        notify(enabled ? "已开启 TUN 接管，下次启动代理时生效。" : "已关闭 TUN，启动代理时改用系统代理。");
    });
    emit selectionChanged();
}

void Bridge::connectOrDisconnect() {
    if (closing_) return;
    if (transitioning_) {
        reject("正在切换代理状态，请稍候。");
        return;
    }
    if (busy_ || savingSelection()) {
        // Second click while queued cancels the queued start instead of piling up.
        auto queued = std::find_if(pending_.begin(), pending_.end(), [](const Pending& item) { return item.label == "切换代理状态"; });
        if (queued != pending_.end()) {
            pending_.erase(queued);
            pendingAction_ = pending_.empty() ? QString{} : pending_.front().label;
            notify("已取消等待中的代理操作。");
            return;
        }
        const bool desired = !connected_;
        defer("切换代理状态", [this, desired] { setProxyActive(desired); });
        return;
    }
    setProxyActive(!connected_);
}

void Bridge::setProxyActive(bool active) {
    if (closing_) return;
    if (busy_ || savingSelection()) {
        defer("切换代理状态", [this, active] { setProxyActive(active); });
        return;
    }
    if (connected_ == active && !engine_.needsCleanup()) {
        notify(active ? "代理已经启动。" : "代理已经关闭。");
        return;
    }
    transitioning_ = true;
    if (!active) {
        work([this] {
            engine_.deactivate();
            return engine_.alive() ? snapshot("代理已关闭，节点列表和测速结果仍然保留。") : QtResult{"代理已关闭。", false, {}};
        });
        return;
    }
    const auto settings = settings_;
    const auto wanted = currentNode();
    clearNodeState();
    work([this, settings, wanted] {
        validate(settings);
        engine_.start(settings, true, wanted.toStdString(), &cancelWork_);
        auto result = snapshot("代理已启动。");
        if (!wanted.isEmpty() && result.group.value("now", std::string{}) != wanted.toStdString())
            result.message = "代理已启动，原节点已不在订阅中，请确认当前节点。";
        result.optimizationSummary = "已应用当前节点 · " + qs(result.group.value("now", std::string{}));
        return result;
    });
}

QVariantMap Bridge::subscription(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= settings_.subscriptions.size()) return {{"name", ""}, {"url", ""}, {"interval", 3600}};
    const auto& sub = settings_.subscriptions[index];
    return {{"name", qs(sub.name)}, {"url", qs(sub.url)}, {"interval", sub.interval}};
}

int Bridge::subscriptionIndex(const QString& id) const {
    for (size_t index = 0; index < settings_.subscriptions.size(); ++index)
        if (qs(settings_.subscriptions[index].id) == id) return static_cast<int>(index);
    return -1;
}

void Bridge::saveSubscription(int index, QString name, QString url, int interval) {
    if (index < -1 || (index >= 0 && static_cast<size_t>(index) >= settings_.subscriptions.size())) {
        reject("订阅不存在。"); return;
    }
    const auto id = index < 0 ? QString{} : qs(settings_.subscriptions[index].id);
    if (!canEditSettings("保存订阅", [this, id, name, url, interval] {
            const int current = id.isEmpty() ? -1 : subscriptionIndex(id);
            if (!id.isEmpty() && current < 0) { reject("原订阅已移除，未保存此修改。"); return; }
            saveSubscription(current, name, url, interval);
        }, id.isEmpty() ? "add-subscription:" + qs(randomToken()) : "save-subscription:" + id)) return;
    guard([&] {
        if (index < -1 || (index >= 0 && static_cast<size_t>(index) >= settings_.subscriptions.size()))
            throw std::runtime_error("订阅不存在");
        Subscription sub{index < 0 ? randomToken() : settings_.subscriptions[index].id, name.trimmed().toStdString(),
                         url.trimmed().toStdString(), index < 0 ? true : settings_.subscriptions[index].enabled, interval};
        // Validate the subscription on its own, before it can disturb the saved set.
        Settings candidate;
        candidate.subscriptions = {sub};
        candidate.subscriptions[0].enabled = true;
        validate(candidate);
        auto updated = settings_;
        if (index < 0) updated.subscriptions.push_back(sub);
        else updated.subscriptions[index] = sub;
        commit(updated);
        emit subscriptionsChanged();
        emit subscriptionSaved();
        notify(index < 0 ? "订阅已添加。" : "订阅已更新。");
        reloadPreview();
    });
}

void Bridge::removeSubscription(int index) {
    if (index < 0 || static_cast<size_t>(index) >= settings_.subscriptions.size()) { reject("订阅不存在。"); return; }
    const auto id = qs(settings_.subscriptions[index].id);
    if (!canEditSettings("移除订阅", [this, id] {
            const int current = subscriptionIndex(id);
            if (current < 0) { notify("该订阅已经移除。"); return; }
            removeSubscription(current);
        }, "remove-subscription:" + id)) return;
    guard([&] {
        if (index < 0 || static_cast<size_t>(index) >= settings_.subscriptions.size()) return;
        auto next = settings_;
        next.subscriptions.erase(next.subscriptions.begin() + index);
        commit(next);
        emit subscriptionsChanged();
        notify("订阅已移除，没有删除服务商侧的任何内容。");
        reloadPreview();
    });
}

void Bridge::enableSubscription(int index, bool enabled) {
    if (index < 0 || static_cast<size_t>(index) >= settings_.subscriptions.size()) { reject("订阅不存在。"); return; }
    const auto id = qs(settings_.subscriptions[index].id);
    if (!canEditSettings(enabled ? "启用订阅" : "停用订阅", [this, id, enabled] {
            const int current = subscriptionIndex(id);
            if (current < 0) { reject("原订阅已移除，未修改其他订阅。"); return; }
            enableSubscription(current, enabled);
        }, "enable-subscription:" + id)) return;
    guard([&] {
        if (index < 0 || static_cast<size_t>(index) >= settings_.subscriptions.size()) return;
        auto next = settings_;
        next.subscriptions[index].enabled = enabled;
        commit(next);
        notify(enabled ? "订阅已启用。" : "订阅已停用，其节点不再参与优选。");
        reloadPreview();
    });
    emit subscriptionsChanged();
}

void Bridge::updateSubscriptions() {
    if (!loaded_) {
        reject("还没有加载节点，先点「读取节点与用量」。");
        return;
    }
    if (busy_) {
        defer("更新订阅", [this] { updateSubscriptions(); });
        return;
    }
    const auto subs = settings_.subscriptions;
    work([this, subs] {
        for (const auto& sub : subs) {
            if (cancelWork_.load()) return QtResult{};
            if (sub.enabled) engine_.request(L"PUT", "/providers/proxies/" + providerKey(sub), nullptr, kProviderUpdateTimeoutMs);
        }
        return snapshot("订阅及节点已更新。");
    });
}

void Bridge::selectNode(QString name) {
    if (closing_) return;
    if (busy_) {
        defer("切换到所选节点", [this, name] { selectNode(name); });
        return;
    }
    if (!loaded_) {
        reject("节点还没加载，先点「加载节点」。");
        return;
    }
    if (!nodes().contains(name)) {
        reject("该节点已不在订阅里，请刷新节点列表。");
        return;
    }
    work([this, name] {
        Json body = {{"name", name.toStdString()}};
        engine_.request(L"PUT", "/proxies/PROXY", &body);
        auto result = snapshot("节点已切换，新连接将使用该节点。");
        result.optimizationSummary = "已手动选择节点 · 本次启动内保持你的选择";
        return result;
    });
}

void Bridge::refreshNodes() {
    if (closing_) return;
    if (busy_) {
        defer("刷新节点", [this] { refreshNodes(); });
        return;
    }
    if (!loaded_) {
        reject("节点还没加载，先点「加载节点」。");
        return;
    }
    work([this] { return snapshot("节点与订阅状态已同步。"); });
}

void Bridge::testNodes() {
    if (!loaded_) {
        reject("节点还没加载，先点「加载节点」。");
        return;
    }
    if (busy_) {
        defer("重新测速优选", [this] { testNodes(); });
        return;
    }
    testing_ = true;
    work([this] { return optimizeNodes(); }, true);
}

void Bridge::loadNodes() {
    if (connected_) {
        reject("代理运行中，节点列表随代理一起同步。");
        return;
    }
    if (busy_) {
        defer("加载节点", [this] { loadNodes(); });
        return;
    }
    const auto settings = settings_;
    clearNodeState();
    testing_ = true;
    work([this, settings] {
        engine_.start(settings, false, {}, &cancelWork_);
        if (cancelWork_.load()) return QtResult{};
        return optimizeNodes();
    }, true);
}

void Bridge::refreshProcesses() {
    if (closing_) return;
    if (catalogBusy_) {
        notify("应用库正在后台刷新，已有应用仍可选择。");
        return;
    }
    auto job = std::make_shared<CatalogJob>();
    const auto dir = dir_;
    catalogBusy_ = true;
    try {
        std::thread([job, dir] {
            try {
            QStringList active;
            for (const auto& path : applicationPaths()) active.push_back(qs(path));
            // Publish the quick window-based scan first so the grid is usable at once.
            auto quick = scanApplicationLibrary({}, {}, active, job->cancel.get());
            {
                std::lock_guard<std::mutex> lock(job->mutex);
                job->quick = quick;
                job->quickReady = true;
            }
            bool complete = false;
            auto rows = installedApplicationLibrary(job->cancel.get(), &complete);
            // A truncated scan must not overwrite the cache with a shorter library.
            if (complete && !job->cancel->load()) {
                try {
                    saveProtected(dir / L"application-cache.dat",
                                  QJsonDocument::fromVariant(rows).toJson(QJsonDocument::Compact).toStdString());
                } catch (...) {
                }
            }
            std::lock_guard<std::mutex> lock(job->mutex);
            job->rows = std::move(rows);
            job->complete = complete;
            job->running = false;
            } catch (const std::exception& error) {
                std::lock_guard<std::mutex> lock(job->mutex);
                job->error = qs(error.what());
                job->running = false;
            } catch (...) {
                std::lock_guard<std::mutex> lock(job->mutex);
                job->error = "读取应用信息失败";
                job->running = false;
            }
        }).detach();
    } catch (const std::exception& e) {
        catalogBusy_ = false;
        message_ = qs(e.what());
        error_ = true;
        emit changed();
        emit catalogStateChanged();
        return;
    }
    catalogJob_ = std::move(job);
    emit catalogStateChanged();
}

void Bridge::applyCatalogResult(QVariantList rows, bool complete) {
    if (complete) baseCatalog_ = std::move(rows);
    else {
        QMap<QString, QVariant> merged;
        for (const auto& row : baseCatalog_) merged.insert(row.toMap()["path"].toString().toCaseFolded(), row);
        for (const auto& row : rows) merged.insert(row.toMap()["path"].toString().toCaseFolded(), row);
        baseCatalog_.clear();
        for (const auto& row : merged) baseCatalog_.push_back(row);
        notify("应用库本次只完成部分扫描，已保留已有应用，可稍后再次刷新。");
    }
    rebuildProcesses();
}

void Bridge::rebuildProcesses() {
    QVariantList rows;
    QSet<QString> seen;
    const auto isManual = [&](const QString& path) {
        return std::any_of(settings_.libraryPaths.begin(), settings_.libraryPaths.end(),
                           [&](const std::string& item) { return qs(item).compare(path, Qt::CaseInsensitive) == 0; });
    };
    for (const auto& item : baseCatalog_) {
        auto row = item.toMap();
        const auto path = row["path"].toString();
        if (path.isEmpty() || backgroundApplication(path) || !QFileInfo(path).isFile()) continue;
        const auto key = path.toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        row["manual"] = isManual(path);
        rows.push_back(row);
    }
    // Library entries and live selections are merged in, so a manual pick survives a rescan.
    auto paths = settings_.libraryPaths;
    for (const auto& path : settings_.processes) paths.push_back(path);
    for (const auto& raw : paths) {
        const auto path = QDir::toNativeSeparators(QDir::cleanPath(qs(raw)));
        const auto key = path.toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        const auto name = applicationName(path);
        rows.push_back(QVariantMap{{"path", path},
                                   {"name", name},
                                   {"filename", QFileInfo(path).fileName()},
                                   {"category", applicationCategory(path, name)},
                                   {"background", backgroundApplication(path)},
                                   {"manual", isManual(path)},
                                   {"installed", false},
                                   {"icon", QString::fromLatin1(path.toUtf8().toHex())},
                                   {"running", false},
                                   {"missing", !QFileInfo(path).isFile()}});
    }
    std::sort(rows.begin(), rows.end(), [](const QVariant& a, const QVariant& b) {
        return QString::localeAwareCompare(a.toMap()["name"].toString(), b.toMap()["name"].toString()) < 0;
    });
    emit catalogWillChange();
    processModel_ = std::move(rows);
    emit processesChanged();
}

void Bridge::toggleProcess(QString path) {
    if (closing_) return;
    if (connected_) {
        reject("代理运行中，先关闭代理再修改程序选择。");
        return;
    }
    // Selection saves are independent of the core, so quiet background work may continue.
    if (busy_ && !syncing_) {
        reject("正在应用设置，程序选择稍后可改。");
        return;
    }
    guard([&] {
        path = QDir::toNativeSeparators(QDir::cleanPath(path));
        auto next = settings_;
        auto existing = std::find_if(next.processes.begin(), next.processes.end(),
                                     [&](const std::string& item) { return qs(item).compare(path, Qt::CaseInsensitive) == 0; });
        if (existing == next.processes.end()) next.processes.push_back(checkedProgramPath(path).toStdString());
        else next.processes.erase(existing);
        commitSelection(next);
        emit selectionChanged();
        notify(next.processes.empty() ? "已恢复全局代理：所有程序都将走代理。"
                                      : "已切换为指定程序代理，其他程序直连。");
    });
}

void Bridge::clearProcesses() {
    if (closing_) return;
    if (connected_) {
        reject("代理运行中，先关闭代理再修改程序选择。");
        return;
    }
    if (busy_ && !syncing_) {
        reject("正在应用设置，程序选择稍后可改。");
        return;
    }
    guard([&] {
        auto next = settings_;
        next.processes.clear();
        commitSelection(next);
        emit selectionChanged();
        notify("已取消全部选择，恢复全局代理。");
    });
}

void Bridge::browseProgram() {
    if (closing_) return;
    if (connected_) {
        reject("代理运行中，先关闭代理再添加应用。");
        return;
    }
    if (busy_ && !syncing_) {
        reject("正在应用设置，稍后再添加应用。");
        return;
    }
    if (savingSelection()) { defer("添加应用", [this] { browseProgram(); }); return; }
    guard([&] {
        const auto picked = pickProgram(hwnd_);
        if (picked.empty()) return;
        const auto path = checkedProgramPath(qs(picked));
        auto next = settings_;
        if (std::none_of(next.libraryPaths.begin(), next.libraryPaths.end(),
                         [&](const std::string& item) { return qs(item).compare(path, Qt::CaseInsensitive) == 0; }))
            next.libraryPaths.push_back(picked);
        if (!selectedPaths().contains(path, Qt::CaseInsensitive)) next.processes.push_back(picked);
        commit(next);
        rebuildProcesses();
        emit selectionChanged();
        notify("应用已加入本机应用库，未运行时也可勾选。");
    });
}

void Bridge::forgetApplication(QString path) {
    if (!canEditSettings("移除手动记录", [this, path] { forgetApplication(path); }, "forget-application:" + path.toCaseFolded())) return;
    guard([&] {
        if (selectedPaths().contains(path, Qt::CaseInsensitive)) throw std::runtime_error("请先取消勾选，再移除手动记录");
        auto next = settings_;
        std::erase_if(next.libraryPaths, [&](const std::string& item) { return qs(item).compare(path, Qt::CaseInsensitive) == 0; });
        commit(next);
        rebuildProcesses();
        notify("已移除手动记录，没有删除应用文件。");
    });
}

void Bridge::revealApplication(QString path) {
    guard([&] {
        if (!QFileInfo(path).isFile()) throw std::runtime_error("应用文件不存在，请重新添加");
        const auto argument = L"/select,\"" + path.toStdWString() + L"\"";
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd_, L"open", L"explorer.exe", argument.c_str(), nullptr, SW_SHOWNORMAL)) <= 32)
            throw std::runtime_error("无法打开应用所在位置");
    });
}

void Bridge::copyApplicationPath(QString path) {
    QGuiApplication::clipboard()->setText(path);
    notify("应用路径已复制。");
}

void Bridge::savePorts(int mixed, int api) {
    if (!canEditSettings("保存端口设置", [this, mixed, api] { savePorts(mixed, api); })) return;
    guard([&] {
        if (mixed < 1024 || mixed > 65535 || api < 1024 || api > 65535 || mixed == api)
            throw std::runtime_error("请输入两个不同的 1024–65535 端口");
        auto next = settings_;
        next.mixedPort = mixed;
        next.apiPort = api;
        commit(next);
        notify("端口设置已保存，重新启动代理后生效。");
        emit selectionChanged();
    });
}

void Bridge::recover() {
    if (connected_) {
        reject("代理正在运行，关闭代理时会自动恢复系统代理。");
        return;
    }
    if (busy_) {
        defer("恢复系统代理", [this] { recover(); });
        return;
    }
    guard([&] {
        engine_.recover();
        notify("系统代理已恢复。");
    });
}

void Bridge::requestQuit() {
    if (closing_) return;
    closing_ = true;
    cancelWork_.store(true);
    if (catalogJob_) catalogJob_->cancel->store(true);
    pending_.clear();
    pendingAction_.clear();
    message_ = "正在安全退出并恢复网络设置…";
    emit changed();
    selectionDebounce_.stop();
    beginSelectionSave();
    if (!busy_) work([this] { engine_.stop(); return QtResult{}; }, true);
}
}
