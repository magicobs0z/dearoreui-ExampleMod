#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/Event.h"
#include "api/types/Id.h"

#include "ll/api/data/CancellableCallback.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace my_mod {

class MyMod;

// Scheme 5: combined state center. Owns the script resource, the declarative
// component UI (panel + tab bar + dynamic body containers), the single
// JS->C++ dispatch target (example.state.init), the three C++->JS event
// streams (hud.tick / log.append / diag.refresh), the real data sources
// (process memory, uptime, push counter, DearOreUI diagnostics) and the page
// lifecycle wiring. The periodic pushes run on the client main thread through
// a self-rescheduling ClientThreadExecutor::executeAfter timer (the LL world
// ClientLevelTickEvent emitter is not ready during mod enable).
class StateCenter {
public:
    StateCenter(dearoreui::api::IDearOreUIApi& api, dearoreui::api::ModId modId, MyMod& mod);
    ~StateCenter();

    StateCenter(StateCenter const&)            = delete;
    StateCenter& operator=(StateCenter const&) = delete;

    /// Registers script + component UI + host method + page subscriptions +
    /// the client-thread push timer. Returns false and cleans up on failure.
    bool registerAll();

    /// Reverses registration order, cancels the timer and unregisters the mod.
    /// Safe to call from disable().
    void shutdown();

    // Host method payload (example.state.init): batch initial snapshot.
    [[nodiscard]] std::string handleInit(std::string_view args);

private:
    void appendLog(std::string level, std::string message);
    void publishStatus();
    void publishLogs();
    void publishDiag();
    void onTimer();
    void stopTimer();
    void onPageReady(dearoreui::api::PageContextView const& view);
    void onPageDestroyed(dearoreui::api::PageContextView const& view);

    [[nodiscard]] std::string jsonEscape(std::string const& value) const;
    [[nodiscard]] std::string collectStatusJson() const;
    [[nodiscard]] std::string collectLogsJson(std::size_t from = 0) const;
    [[nodiscard]] std::string collectDiagJson(std::size_t limit = 200) const;
    [[nodiscard]] std::string collectMemoryMb() const;

    dearoreui::api::IDearOreUIApi& mApi;
    dearoreui::api::ModId          mModId;
    MyMod&                         mMod;

    // Handles.

    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
    std::optional<dearoreui::api::RegistrationHandle> mHostMethodHandle;
    std::optional<dearoreui::api::SubscriptionHandle> mReadySub;
    std::optional<dearoreui::api::SubscriptionHandle> mDestroyedSub;
    std::shared_ptr<ll::data::CancellableCallback>    mTimer;

    // Live data (client main thread only).
    std::optional<dearoreui::api::ContextId> mContextId;
    std::chrono::steady_clock::time_point    mStarted{};
    std::uint64_t                            mPushes{0};
    std::uint64_t                            mTimerRounds{0};
    std::size_t                              mLastLogIndex{0};
    struct LogLine {
        std::uint64_t timestampMs;
        std::string   level;
        std::string   message;
    };
    std::deque<LogLine> mLogs; // ring, cap 256
};

} // namespace my_mod