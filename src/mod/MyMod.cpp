#include "mod/MyMod.h"

#include "mod/examples/ExampleFactory.h"

#include "bridge/DearOreUIBridge.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "ll/api/thread/InterruptableSleep.h"

#include <chrono>
#include <fstream>
#include <sstream>
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
    // Select the active tutorial example from mod dir config.json
    // ({"example":"07"} default; "01".."07"; unknown ids fall back to 07).
    std::string configJson;
    try {
        auto configPath = getSelf().getModDir() / "config.json";
        std::ifstream stream(configPath);
        if (stream) {
            std::stringstream buffer;
            buffer << stream.rdbuf();
            configJson = buffer.str();
            getSelf().getLogger().info("Read config from {}", configPath.string());
        } else {
            getSelf().getLogger().info("No config.json at {}; defaults to example 07 (state center).", configPath.string());
        }
    } catch (...) {
        getSelf().getLogger().warn("Failed to read config.json; defaults to example 07.");
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
        mRetryThread = std::jthread([this, configJson](std::stop_token stop) {
            ll::thread::InterruptableSleep sleeper;
            int                            polls = 0;
            while (!stop.stop_requested() && !mRetryStop.load() && polls < kMaxRetryPolls) {
                auto candidate = queryApi(1);
                if (candidate.status == DearOreUIBridge_Ok) {
                    void* apiPtr = candidate.api;
                    getSelf().getLogger().info("DearOreUI ready, handing API to main thread.");
                    ll::thread::ClientThreadExecutor::getDefault().execute([this, apiPtr, configJson]() {
                        if (mRetryStop.load()) return;
                        mOreui = static_cast<dearoreui::api::IDearOreUIApi*>(apiPtr);
                        if (!startExample(configJson)) {
                            getSelf().getLogger().error("Example registration failed.");
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
    mOreui = static_cast<dearoreui::api::IDearOreUIApi*>(bridge.api);
    if (!startExample(configJson)) {
        getSelf().getLogger().error("Example registration failed.");
        mOreui = nullptr;
        return false;
    }
    return true;
}

void MyMod::disconnectDearOreUI() {
    if (mExample) {
        mExample->shutdown();
        mExample.reset();
    }
    mModId = dearoreui::api::ModId{};
    mOreui = nullptr;
}

// Builds the active tutorial example via the factory and registers it.
bool MyMod::startExample(std::string const& configJson) {
    mExample = examples::ExampleFactory::create(*mOreui, *this, configJson);
    if (!mExample) {
        return false;
    }
    if (!mExample->registerAll()) {
        getSelf().getLogger().error("Example '{}' registration failed.", mExample->name());
        mExample.reset();
        return false;
    }
    getSelf().getLogger().info("Example '{}' is active.", mExample->name());
    return true;
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