#include "mod/MyMod.h"

#include "mod/StateCenter.h"

#include "bridge/DearOreUIBridge.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "ll/api/thread/InterruptableSleep.h"

#include <chrono>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace my_mod {

namespace {

// Resolves the pure C bridge export from the loaded DearOreUI.dll without
// linking its import library: the API contract is the bridge symbol only.
DearOreUIBridgeResult (*queryApi)(uint32_t) = nullptr;

bool loadBridge() {
#ifdef _WIN32
    HMODULE module = GetModuleHandleW(L"DearOreUI.dll");
    if (module == nullptr) return false;
    queryApi = reinterpret_cast<DearOreUIBridgeResult (*)(uint32_t)>(GetProcAddress(module, "DearOreUI_QueryApi"));
    return queryApi != nullptr;
#else
    return false;
#endif
}

// Ready-retry bound: 20 polls x 500ms = 10s. With the LL `dependencies`
// declaration this path is normally never taken.
constexpr int                kMaxRetryPolls = 20;
constexpr std::chrono::milliseconds kRetryInterval{500};

} // namespace

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

bool MyMod::connectDearOreUI() {
    if (queryApi == nullptr && !loadBridge()) {
        getSelf().getLogger().warn("DearOreUI bridge export is not available.");
        return false;
    }
    auto bridge = queryApi(1);
    if (bridge.status != DearOreUIBridge_Ok) {
        switch (bridge.status) {
        case DearOreUIBridge_VersionMismatch:
            getSelf().getLogger().warn("DearOreUI protocol mismatch: expected 1, current {}", bridge.protocolVersion);
            break;
        default:
            getSelf().getLogger().warn("DearOreUI is not ready; waiting for it (retry loop).");
            break;
        }
        // Defensive retry: poll the thread-safe bridge query; hand the API to
        // the client main thread when it becomes ready.
        mRetryStop  = false;
        mRetryThread = std::jthread([this](std::stop_token stop) {
            ll::thread::InterruptableSleep sleeper;
            int                            polls = 0;
            while (!stop.stop_requested() && !mRetryStop.load() && polls < kMaxRetryPolls) {
                auto candidate = queryApi(1);
                if (candidate.status == DearOreUIBridge_Ok) {
                    void* apiPtr = candidate.api;
                    getSelf().getLogger().info("DearOreUI ready, handing API to main thread.");
                    ll::thread::ClientThreadExecutor::getDefault().execute([this, apiPtr]() {
                        if (mRetryStop.load()) return;
                        mOreui = static_cast<dearoreui::api::IDearOreUIApi*>(apiPtr);
                        mModId = dearoreui::api::ModId{"example.state_center"};
                        mStateCenter = std::make_unique<StateCenter>(*mOreui, mModId, *this);
                        if (!mStateCenter->registerAll()) {
                            getSelf().getLogger().error("StateCenter registration failed.");
                            mStateCenter.reset();
                            mOreui = nullptr;
                        }
                    });
                    return;
                }
                ++polls;
                sleeper.sleepFor(kRetryInterval);
            }
            if (polls >= kMaxRetryPolls) {
                getSelf().getLogger().warn("DearOreUI did not become ready within the retry window.");
            }
        });
        return false;
    }
    getSelf().getLogger().info("Connected to DearOreUI, protocol {}", bridge.protocolVersion);
    mOreui       = static_cast<dearoreui::api::IDearOreUIApi*>(bridge.api);
    mModId       = dearoreui::api::ModId{"example.state_center"};
    mStateCenter = std::make_unique<StateCenter>(*mOreui, mModId, *this);
    if (!mStateCenter->registerAll()) {
        getSelf().getLogger().error("StateCenter registration failed.");
        mStateCenter.reset();
        mOreui = nullptr;
        return false;
    }
    return true;
}

void MyMod::disconnectDearOreUI() {
    if (mStateCenter) {
        mStateCenter->shutdown();
        mStateCenter.reset();
    }
    mModId = dearoreui::api::ModId{};
    mOreui = nullptr;
}

bool MyMod::enable() {
    getSelf().getLogger().debug("Enabling...");
    connectDearOreUI();
    // Keep the mod enabled even when DearOreUI is not ready yet: the retry
    // loop (or the next client restart) establishes the connection.
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    mRetryStop = true;
    if (mRetryThread.joinable()) {
        mRetryThread.join();
    }
    disconnectDearOreUI();
    return true;
}

} // namespace my_mod

LL_REGISTER_MOD(my_mod::MyMod, my_mod::MyMod::getInstance());