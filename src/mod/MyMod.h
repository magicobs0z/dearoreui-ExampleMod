#pragma once

#include "ll/api/mod/NativeMod.h"

#include "api/IDearOreUIApi.h"
#include "api/types/Event.h"
#include "api/types/HostMethodManifest.h"
#include "api/types/Page.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace my_mod {

class MyMod {

public:
    static MyMod& getInstance();

    MyMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

private:
    /// Queries the bridge once (thread-safe, does not modify members). Returns
    /// the bridge result; `bridgeLoaded` reports whether the symbol resolved.
    struct ApiAcquire {
        unsigned char status;        // DearOreUIBridgeStatus
        unsigned char bridgeLoaded;  // bool
        unsigned int  protocolVersion;
        void*         api;
    };
    ApiAcquire acquireApi();

    /// Registers this mod's capabilities with DearOreUI. Must run on the
    /// client main thread (all mutation calls). Assumes the bridge returned Ok.
    bool performRegistration();

    /// Called from enable() on the main thread. On Ok registers immediately;
    /// otherwise starts the background readiness retry loop.
    bool connectDearOreUI();

    /// Starts the background retry thread: polls QueryApi until Ok (or
    /// timeout), then hands the api pointer to the client main thread.
    void startReadyRetry();

    void disconnectDearOreUI();

    ll::mod::NativeMod& mSelf;

    // DearOreUI API instance; owned by DearOreUI, never deleted here. Written
    // only on the client main thread.
    dearoreui::api::IDearOreUIApi* mOreui{nullptr};
    dearoreui::api::ModId          mModId;

    // Handles owned by this mod; cleaned up in disconnectDearOreUI().
    std::optional<dearoreui::api::RegistrationHandle> mHostMethodHandle;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
    std::optional<dearoreui::api::SubscriptionHandle> mPageSubscription;

    // Latest page context captured from the Ready callback; the context used
    // by publishEvent (C++ -> JS pushes) and by the page JS dispatch.
    std::optional<dearoreui::api::ContextId> mContextId;

    // Retry lifecycle.
    std::atomic<bool>  mRetryStop{false};
    std::jthread       mRetryThread;

    // Publishes the current status snapshot to the UI (hud.tick).
    void publishStatusSnapshot();
};

} // namespace my_mod