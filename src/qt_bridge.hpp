#pragma once
#include "engine.hpp"
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <memory>

namespace muse {

// Snapshot returned by a background task. Everything the UI needs after the
// work finishes, so the polling loop never has to ask the core twice.
struct QtResult {
    QString message;
    bool connected = false;
    Json group = Json::object(), proxies = Json::object(), providers = Json::object(), delays = Json::object();
    QString optimizationSummary;
};

class Bridge : public QObject {
    Q_OBJECT
    friend struct BridgeTestAccess;
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool error READ error NOTIFY changed)
    Q_PROPERTY(bool admin READ admin CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(int mode READ mode NOTIFY selectionChanged)
    Q_PROPERTY(bool tunEnabled READ tunEnabled NOTIFY selectionChanged)
    Q_PROPERTY(int mixedPort READ mixedPort NOTIFY changed)
    Q_PROPERTY(int apiPort READ apiPort NOTIFY changed)
    Q_PROPERTY(QVariantList subscriptions READ subscriptions NOTIFY subscriptionsChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY processesChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY selectionChanged)
    Q_PROPERTY(QStringList nodes READ nodes NOTIFY changed)
    Q_PROPERTY(QString currentNode READ currentNode NOTIFY changed)
    Q_PROPERTY(QVariantList nodeDetails READ nodeDetails NOTIFY nodesChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(QVariantList applicationCategories READ applicationCategories NOTIFY processesChanged)
    Q_PROPERTY(QStringList nodeRegions READ nodeRegions NOTIFY nodesChanged)
    Q_PROPERTY(QString optimizationSummary READ optimizationSummary NOTIFY changed)
    Q_PROPERTY(bool catalogBusy READ catalogBusy NOTIFY catalogStateChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY changed)
    Q_PROPERTY(bool closing READ closing NOTIFY changed)
    Q_PROPERTY(bool savingSelection READ savingSelection NOTIFY selectionStateChanged)
    Q_PROPERTY(bool transitioning READ transitioning NOTIFY changed)
    // True when a settings, subscription or selection change would be refused.
    // lockReason explains why, so the UI can say it instead of turning dead.
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(QString lockReason READ lockReason NOTIFY changed)

    fs::path dir_;
    Settings settings_;
    bool settingsReadFailed_ = false;
    Settings selectionCheckpoint_, writtenSelection_;
    Engine engine_;

    QTimer timer_;
    QTimer selectionDebounce_;
    std::future<void> selectionSave_;
    bool selectionDirty_ = false;
    bool selectionSaveError_ = false;

    std::future<QtResult> task_;
    std::atomic_bool cancelWork_{false};
    // The application scan runs detached behind a shared result slot: a shell call
    // that hangs inside the scan must never block polling or shutdown.
    struct CatalogJob;
    std::shared_ptr<CatalogJob> catalogJob_;
    bool catalogBusy_ = false;
    bool closing_ = false;
    bool transitioning_ = false;
    QString pendingAction_;
    struct Pending { QString label; std::function<void()> action; QString key; };
    std::deque<Pending> pending_;

    bool connected_ = false, busy_ = false, error_ = false;
    QString message_;
    QString optimizationSummary_ = "启动后自动测速，并优选最低延迟节点";
    Json group_ = Json::object();
    Json proxies_ = Json::object(), providers_ = Json::object();
    Json delays_ = Json::object();
    bool testing_ = false;
    bool syncing_ = false;
    bool loaded_ = false;
    bool autoManage_ = false;
    int refreshTicks_ = 0;
    uint64_t operationGeneration_ = 0;
    QVariantList processModel_;
    QVariantList baseCatalog_;
    HWND hwnd_ = nullptr;

    // Background task plumbing.
    void work(std::function<QtResult()> fn, bool quiet = false);
    void guard(std::function<void()> fn);
    void poll();
    void pollSelectionSave();
    void pollCatalogTask();
    void pollWorkTask();
    bool pollShutdown();
    void pollQueue();
    void pollEngineHealth();
    void pollPeriodicRefresh();

    // Selection persistence.
    void commit(Settings settings);
    void commitSelection(Settings settings);
    void beginSelectionSave();
    void finishSelectionSave();
    void flushSelection();
    void rollbackSelection();

    // Feedback helpers.
    void notify(QString message);
    void reject(QString message, QString queue = {});
    bool canEditSettings(const QString& action, std::function<void()> retry, QString key = {});
    int subscriptionIndex(const QString& id) const;
    void setProxyActive(bool active);

    void rebuildProcesses();
    void applyCatalogResult(QVariantList rows, bool complete);
    void clearNodeState();
    void defer(QString label, std::function<void()> action, QString key = {});
    void reloadPreview();
    QtResult snapshot(QString message);
    QtResult optimizeNodes();

public:
    explicit Bridge(QObject* parent = nullptr);
    Bridge(fs::path directory, bool recover, QObject* parent = nullptr);
    ~Bridge() override;
    void setWindow(HWND hwnd) { hwnd_ = hwnd; }
    bool connected() const { return connected_; }
    bool busy() const { return busy_; }
    bool error() const { return error_; }
    bool admin() const { return isAdmin(); }
    QString version() const;
    QString message() const { return message_; }
    QString optimizationSummary() const { return testing_ ? QString("正在测速并优选，代理开关保持不变…") : optimizationSummary_; }
    int mode() const { return effectiveRoute(settings_) == RouteMode::Selected ? 2 : settings_.tun ? 1 : 0; }
    bool tunEnabled() const { return settings_.tun; }
    int mixedPort() const { return settings_.mixedPort; }
    int apiPort() const { return settings_.apiPort; }
    QVariantList subscriptions() const;
    QVariantList processes() const { return processModel_; }
    QVariantList applicationCategories() const;
    QStringList nodeRegions() const;
    QStringList selectedPaths() const;
    QStringList nodes() const;
    QString currentNode() const;
    QVariantList nodeDetails() const;
    bool testing() const { return testing_; }
    bool syncing() const { return syncing_; }
    bool loaded() const { return loaded_; }
    bool catalogBusy() const { return catalogBusy_; }
    bool closing() const { return closing_; }
    QString pendingAction() const { return pendingAction_; }
    bool savingSelection() const { return selectionDirty_ || selectionSave_.valid(); }
    bool transitioning() const { return transitioning_; }
    bool locked() const { return !lockReason().isEmpty(); }
    QString lockReason() const;
    Q_INVOKABLE void setTun(bool enabled);
    Q_INVOKABLE void connectOrDisconnect();
    Q_INVOKABLE QVariantMap subscription(int index) const;
    Q_INVOKABLE void saveSubscription(int index, QString name, QString url, int interval);
    Q_INVOKABLE void removeSubscription(int index);
    Q_INVOKABLE void enableSubscription(int index, bool enabled);
    Q_INVOKABLE void updateSubscriptions();
    Q_INVOKABLE void selectNode(QString name);
    Q_INVOKABLE void refreshNodes();
    Q_INVOKABLE void testNodes();
    Q_INVOKABLE void loadNodes();
    Q_INVOKABLE void refreshProcesses();
    Q_INVOKABLE void toggleProcess(QString path);
    Q_INVOKABLE void clearProcesses();
    Q_INVOKABLE void browseProgram();
    Q_INVOKABLE void forgetApplication(QString path);
    Q_INVOKABLE void revealApplication(QString path);
    Q_INVOKABLE void copyApplicationPath(QString path);
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void savePorts(int mixed, int api);
    Q_INVOKABLE void recover();
    Q_INVOKABLE void clearMessage() { message_.clear(); error_ = false; emit changed(); }

signals:
    void changed();
    void selectionChanged();
    void subscriptionsChanged();
    void processesChanged();
    void catalogWillChange();
    void nodesChanged();
    void subscriptionSaved();
    void catalogStateChanged();
    void readyToQuit();
    void selectionStateChanged();
};
}
