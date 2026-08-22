#pragma once

#include "ll/api/mod/NativeMod.h"

#include "api/IDearOreUIApi.h"
#include "api/types/Id.h"

#include "mod/examples/ExampleBase.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

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
    /// Acquires the DearOreUI API instance through the pure C bridge and
    /// starts the config-selected tutorial example. Returns false when the
    /// bridge is unavailable.
    bool connectDearOreUI();

    void disconnectDearOreUI();

    /// Builds the active example (config.json "example" key, default "07")
    /// via ExampleFactory and registers it.
    bool startExample(std::string const& configJson);

    ll::mod::NativeMod& mSelf;

    // DearOreUI API instance; owned by DearOreUI, never deleted here. Written
    // only on the client main thread.
    dearoreui::api::IDearOreUIApi* mOreui{nullptr};
    dearoreui::api::ModId          mModId;

    // The active tutorial example (one at a time).
    std::unique_ptr<examples::ExampleBase> mExample;

    // Retry lifecycle (defensive; LL dependency ordering normally connects
    // on the first attempt).
    std::atomic<bool> mRetryStop{false};
    std::jthread      mRetryThread;
};

} // namespace my_mod