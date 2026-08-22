#include "mod/MyMod.h"

#include "api/IHostMethod.h"
#include "api/manifest/UiManifest.h"
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

// Ready-retry bound: 20 polls x 500ms = 10s. LeviLamina enables mods in
// directory order, so my-mod.enable() normally runs before DearOreUI.enable().
constexpr int kMaxRetryPolls = 20;
constexpr auto kRetryInterval = std::chrono::milliseconds(500);

// The minimal-connect host method: called exactly once by the page JS through
// the single allowed JS->C++ dispatch (StateDemo.init).
class StateInitMethod final : public dearoreui::api::IHostMethod {
public:
    [[nodiscard]] std::string name() const override { return "example.state.init"; }
    [[nodiscard]] dearoreui::api::Permission requiredPermission() const override {
        return dearoreui::api::Permission::HostReadOnly;
    }
    [[nodiscard]] dearoreui::api::Result<std::string>
    execute(dearoreui::api::ContextId contextId, std::string_view args) override {
        return dearoreui::api::Result<std::string>::success(
            "{\"source\":\"cpp\",\"method\":\"example.state.init\",\"context\":"
            + std::to_string(contextId.value())
            + ",\"message\":\"minimal connect ok\",\"received\":" + std::string(args) + "}"
        );
    }
};

// Minimal-connect UI: one status card. The page JS subscribes to hud.tick and
// performs the single dispatch once. Payloads from C++ are JSON strings.
constexpr char kMinimalHtmlBody[] = R"html(
<div id="sc-root" style="position:fixed;top:8px;left:8px;padding:10px;background:rgba(16,20,28,0.92);border:1px solid rgba(120,180,255,0.35);border-radius:8px;color:#dbe4f0;font-family:monospace;font-size:12px;min-width:240px;z-index:2147483000;">
  <div style="font-weight:bold;margin-bottom:6px;">DearOreUI 最小连接验证</div>
  <div id="sc-status" style="white-space:pre-wrap;">connecting...</div>
</div>
<script>
(function () {
  function render(payload) {
    try { var data = JSON.parse(payload); }
    catch (e) { return; }
    var el = document.getElementById('sc-status');
    if (el) el.textContent = 'message: ' + data.message + ' | source: ' + data.source + ' | tick: ' + data.tick;
  }
  window.oreui.event.on('hud.tick', render);
  try {
    window.oreui.host.call('example.state.init', {"wants": "snapshot"}).then(function (res) {
      try { render(res); } catch (e) {}
    }).catch(function () {
      try { render('{"message":"init dispatch failed"}'); } catch (e) {}
    });
  } catch (e) {}
})();
</script>
)html";

} // namespace

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

void MyMod::publishStatusSnapshot() {
    if (mOreui == nullptr || !mContextId.has_value()) return;
    dearoreui::api::EventPublishOptions options;
    options.owner   = mModId;
    options.context = *mContextId;
    options.name    = "hud.tick";
    options.payload =
        "{\"source\":\"cpp\",\"message\":\"minimal connect ok\",\"tick\":1,\"context\":"
        + std::to_string(mContextId->value()) + "}";
    auto result = mOreui->publishEvent(options);
    if (result.isErr()) {
        getSelf().getLogger().error("publishEvent failed: {}", result.error().message);
    }
}

MyMod::ApiAcquire MyMod::acquireApi() {
    ApiAcquire acquired{};
    if (queryApi == nullptr && !loadBridge()) {
        acquired.status = DearOreUIBridge_ModNotLoaded;
        return acquired;
    }
    auto bridge = queryApi(1);
    acquired.bridgeLoaded    = 1;
    acquired.status          = static_cast<unsigned char>(bridge.status);
    acquired.protocolVersion = bridge.protocolVersion;
    acquired.api             = bridge.api;
    return acquired;
}

bool MyMod::performRegistration() {
    if (mOreui == nullptr || mRetryStop.load()) return false;
    getSelf().getLogger().info("Registering with DearOreUI...");

    // 1. Mod identity. id == modNamespace (DearOreUI owner-namespace contract);
// dotted names are valid for both.
    dearoreui::api::ModManifest manifest;
    manifest.id           = dearoreui::api::ModId{"example.state_center"};
    manifest.modNamespace = "example.state_center";
    manifest.displayName  = "State Center Example Mod";
    manifest.modVersion   = dearoreui::api::Version{1, 0, 0};
    manifest.permissions  = {
        dearoreui::api::Permission::HostReadOnly,
        dearoreui::api::Permission::PageObserve,
        dearoreui::api::Permission::UiMount,
    };
    auto mod = mOreui->registerMod(manifest);
    if (mod.isErr()) {
        getSelf().getLogger().error("registerMod failed: {}", mod.error().message);
        mOreui = nullptr;
        return false;
    }
    mModId = manifest.id;

    // 2. Host method (single JS->C++ dispatch target).
    dearoreui::api::HostMethodManifest hostManifest;
    hostManifest.name          = "example.state.init";
    hostManifest.pageScopes    = {dearoreui::api::PageScope::Any};
    hostManifest.permissions   = dearoreui::api::PermissionSet{std::vector{dearoreui::api::Permission::HostReadOnly}};
    auto host = mOreui->registerHostMethod(mModId, hostManifest, std::make_shared<StateInitMethod>());
    if (host.isErr()) {
        getSelf().getLogger().error("registerHostMethod failed: {}", host.error().message);
        static_cast<void>(mOreui->unregisterMod(mModId));
        mOreui = nullptr;
        return false;
    }
    mHostMethodHandle = host.value();

    // 3. Minimal overlay UI.
    dearoreui::api::UiManifest uiManifest;
    uiManifest.modNamespace  = manifest.modNamespace;
    uiManifest.id            = "minimal_connect";
    uiManifest.kind          = dearoreui::api::UiKind::Overlay;
    uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
    uiManifest.anchor        = dearoreui::api::UiAnchor::TopLeft;
    uiManifest.pointerEvents = false;
    uiManifest.containerId =
        dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
    uiManifest.fingerprint = "example.state_center.minimal_connect.1";
    auto ui = mOreui->registerOverlay(mModId, uiManifest, std::string(kMinimalHtmlBody));
    if (ui.isErr()) {
        getSelf().getLogger().error("registerOverlay failed: {}", ui.error().message);
        static_cast<void>(mOreui->unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mOreui->unregisterMod(mModId));
        mOreui = nullptr;
        return false;
    }
    mUiHandle = ui.value();

    // 4. Page lifecycle: capture the first Ready context and push the initial
    // status snapshot over C++ -> JS.
    dearoreui::api::PageSubscriptionOptions subOptions;
    subOptions.owner  = mModId;
    subOptions.scopes = {dearoreui::api::PageScope::Any};
    auto sub = mOreui->subscribePage(
        subOptions,
        dearoreui::api::PageEvent::Ready,
        [this](dearoreui::api::PageContextView const& view) {
            mContextId = view.id;
            getSelf().getLogger().info("page ready, context {}", view.id.value());
            publishStatusSnapshot();
        }
    );
    if (sub.isErr()) {
        getSelf().getLogger().error("subscribePage failed: {}", sub.error().message);
        static_cast<void>(mOreui->unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mOreui->unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mOreui->unregisterMod(mModId));
        mOreui = nullptr;
        return false;
    }
    mPageSubscription = sub.value();
    getSelf().getLogger().info("DearOreUI minimal connect registered.");
    return true;
}

void MyMod::startReadyRetry() {
    if (mRetryThread.joinable()) return;
    mRetryStop = false;
    mRetryThread = std::jthread([this](std::stop_token stop) {
        ll::thread::InterruptableSleep sleeper;
        int                            polls = 0;
        while (!stop.stop_requested() && !mRetryStop.load() && polls < kMaxRetryPolls) {
            auto acquired = acquireApi();
            if (acquired.status == DearOreUIBridge_Ok) {
                void* apiPtr = acquired.api;
                getSelf().getLogger().info("DearOreUI ready, handing API to main thread.");
                // Mutation calls (registerMod & co) must run on the client
                // main thread; the retry thread never writes members.
                ll::thread::ClientThreadExecutor::getDefault().execute([this, apiPtr]() {
                    if (mRetryStop.load()) return;
                    mOreui = static_cast<dearoreui::api::IDearOreUIApi*>(apiPtr);
                    performRegistration();
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
}

bool MyMod::connectDearOreUI() {
    auto acquired = acquireApi();
    if (acquired.status != DearOreUIBridge_Ok) {
        switch (acquired.status) {
        case DearOreUIBridge_NotReady:
            getSelf().getLogger().warn(
                "DearOreUI is loading; waiting for it to become ready (retry loop started)."
            );
            break;
        case DearOreUIBridge_VersionMismatch:
            getSelf().getLogger().warn("DearOreUI protocol mismatch: expected 1, current {}", acquired.protocolVersion);
            break;
        default:
            getSelf().getLogger().warn("DearOreUI bridge is not available; retry loop started.");
            break;
        }
        startReadyRetry();
        return false;
    }
    getSelf().getLogger().info("Connected to DearOreUI, protocol {}", acquired.protocolVersion);
    mOreui = static_cast<dearoreui::api::IDearOreUIApi*>(acquired.api);
    return performRegistration();
}

void MyMod::disconnectDearOreUI() {
    if (mOreui == nullptr) return;
    if (mPageSubscription.has_value()) {
        static_cast<void>(mOreui->unsubscribePage(*mPageSubscription));
        mPageSubscription.reset();
    }
    if (mUiHandle.has_value()) {
        static_cast<void>(mOreui->unregisterUi(*mUiHandle));
        mUiHandle.reset();
    }
    if (mHostMethodHandle.has_value()) {
        static_cast<void>(mOreui->unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
    }
    mContextId.reset();
    static_cast<void>(mOreui->unregisterMod(mModId));
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