#include "engine.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace muse {
namespace {

constexpr int kConfigCheckTimeoutMs = 15000;  // mihomo -t validates the generated config
constexpr int kReadyPolls = 60;               // 60 * 150 ms => wait up to 9 s for the controller
constexpr int kReadyDelayMs = 150;
constexpr int kNodePolls = 30;                // 30 * 500 ms => wait up to 15 s for the first nodes
constexpr int kNodeDelayMs = 500;
constexpr int kTeardownSettleMs = 400;        // let the core drop its TUN adapter and routes

void sleepFor(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

} // namespace

Engine::~Engine() {
    try { stop(); } catch (...) {}
}

void Engine::recover() {
    proxy_.restore();
    std::error_code ec;
    fs::remove(runtime_, ec);
}

void Engine::start(const Settings& settings, bool activate, const std::string& preferredNode, const std::atomic_bool* cancel) {
    const auto ensureActive = [cancel] { if (cancel && cancel->load()) throw std::runtime_error("操作已取消"); };
    ensureActive();
    validate(settings);
    if (activate && (settings.tun || effectiveRoute(settings) == RouteMode::Selected) && !isAdmin())
        throw std::runtime_error("全局 TUN／程序分流需要管理员权限。请关闭 Muse，右键以管理员身份运行。");
    const auto exe = exeDir() / L"core" / L"mihomo.exe";
    if (!fs::exists(exe)) throw std::runtime_error("缺少 core/mihomo.exe，请先运行 scripts/install-core.ps1");

    stop();
    secret_ = randomToken();
    port_ = settings.apiPort;
    fs::create_directories(dir_ / L"providers");

    auto runtime = settings;
    if (!activate) {
        // Preview mode only harvests nodes; it must never touch the machine's traffic.
        runtime.tun = false;
        runtime.processes.clear();
    }
    auto config = makeConfig(runtime, secret_);
    // Load subscriptions and select the requested node before changing routes.
    config["tun"]["enable"] = false;
    writeFile(runtime_, config.dump(2));

    try {
        // First pass: let the core validate the generated config so errors surface
        // as a readable message instead of a silently dead proxy.
        child_.launch(exe, runtime_, dir_, true);
        DWORD code = 0;
        if (!child_.wait(kConfigCheckTimeoutMs, code) || code != 0)
            throw std::runtime_error("内核配置校验失败，请检查订阅和端口设置");

        child_.launch(exe, runtime_, dir_);
        bool ready = false;
        for (int i = 0; i < kReadyPolls && !ready; ++i) {
            ensureActive();
            if (!child_.alive()) throw std::runtime_error("内核启动后退出：请检查端口占用、TUN 驱动及防火墙");
            try {
                request(L"GET", "/version");
                ready = true;
            } catch (...) {
                sleepFor(kReadyDelayMs);
            }
        }
        if (!ready) throw std::runtime_error("内核启动超时");

        // Never redirect the machine to an empty subscription group.
        bool hasNodes = false;
        Json group;
        for (int i = 0; i < kNodePolls && !hasNodes; ++i) {
            ensureActive();
            if (!child_.alive()) throw std::runtime_error("内核意外退出");
            group = request(L"GET", "/proxies/PROXY");
            if (group.contains("all"))
                for (const auto& node : group["all"])
                    if (node != "COMPATIBLE" && node != "DIRECT" && node != "REJECT") hasNodes = true;
            if (!hasNodes) sleepFor(kNodeDelayMs);
        }
        if (!hasNodes) throw std::runtime_error("订阅没有可用节点：请确认链接为 Clash／mihomo 订阅且可以访问");
        ensureActive();

        if (!preferredNode.empty() && group.contains("all") &&
            std::find(group["all"].begin(), group["all"].end(), preferredNode) != group["all"].end()) {
            Json choice = {{"name", preferredNode}};
            request(L"PUT", "/proxies/PROXY", &choice);
        }
        if (activate) {
            ensureActive();
            if (settings.tun || effectiveRoute(settings) == RouteMode::Selected) {
                Json on = {{"tun", {{"enable", true}}}};
                request(L"PATCH", "/configs", &on);
            } else proxy_.enable(settings.mixedPort);
        }
        active_ = activate;
    } catch (...) {
        const auto error = std::current_exception();
        try { stop(); } catch (...) {}
        std::rethrow_exception(error);
    }
}

void Engine::deactivate() {
    if (!child_.alive()) { stop(); return; }
    try {
        if (child_.alive()) {
            Json off = {{"tun", {{"enable", false}}}};
            request(L"PATCH", "/configs", &off);
            sleepFor(kTeardownSettleMs);
        }
        proxy_.restore();
        active_ = false;
    } catch (...) {
        const auto error = std::current_exception();
        try { stop(); } catch (...) {}
        std::rethrow_exception(error);
    }
}

void Engine::stop() {
    active_ = false;
    // Restore the previous system proxy even when the child has already exited.
    std::exception_ptr error;
    try {
        proxy_.restore();
    } catch (...) {
        error = std::current_exception();
    }
    if (child_.alive()) {
        // Ask for a TUN shutdown first: killing the process outright can leave the
        // adapter and its routes behind, which breaks the next start.
        try {
            Json off = {{"tun", {{"enable", false}}}};
            request(L"PATCH", "/configs", &off);
        } catch (...) {
        }
        sleepFor(kTeardownSettleMs);
    }
    child_.stop();
    std::error_code ec;
    fs::remove(runtime_, ec);
    secret_.clear();
    port_ = 0;
    if (error) std::rethrow_exception(error);
}

Json Engine::request(const std::wstring& verb, const std::string& path, const Json* body, int timeoutMs) {
    const auto result = http(port_, secret_, verb, path, body ? body->dump() : "", timeoutMs);
    return result.empty() ? Json::object() : Json::parse(result);
}
}
