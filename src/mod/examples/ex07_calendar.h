#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/Event.h"
#include "api/types/Id.h"

#include "mod/examples/ExampleBase.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace my_mod {
namespace examples {

// Example 07 - Full Calendar (graduation project).
//
// Combines everything from lessons 01-06 into one polished calendar:
//   * month grid with paging / today highlight / click-to-select (lesson 03),
//   * C++->JS pushes: seed events on page ready + a frame-driven clock bar
//     and cross-midnight today refresh (lessons 04/05),
//   * event dots + detail sidebar with local add/delete (JS-side store),
//   * a registered host method "calendar.init" kept as the single-dispatch
//     capability, documented but NOT invoked (the page keeps its dispatch
//     budget unused; all data flows C++->JS, per the engine contract).
//
// Engine constraints honored (verified on the real client):
//   * page script never dispatches JS->C++; everything here is event-driven.
//   * Periodic pushes run on the DearOreUI client-frame service (game main
//     loop, main menu included): tick emitters are not ready during enable,
//     executeAfter/jthread did not fire, ll coroutine executor does not run.
//   * Chinese text is written by the page script with Noto Sans; component
//     static labels stay ASCII.
class CalendarExample final : public ExampleBase {
public:
    CalendarExample(dearoreui::api::IDearOreUIApi& api, MyMod& mod);
    ~CalendarExample() override;

    CalendarExample(CalendarExample const&)            = delete;
    CalendarExample& operator=(CalendarExample const&) = delete;

    /// Registers script + component UI + host method + page subscriptions.
    /// Returns false and cleans up on failure.
    [[nodiscard]] bool registerAll() override;

    /// Reverses registration order, stops the frame subscription and
    /// unregisters the mod. Safe to call from disable().
    void shutdown() override;

    /// Batch seed-event snapshot for the single-dispatch capability
    /// (registered for demonstration; the page script does not call it).
    [[nodiscard]] std::string handleInit() const;

    [[nodiscard]] std::string_view name() const override { return "07-calendar"; }

private:
    void onPageReady(dearoreui::api::PageContextView const& view);
    void onPageDestroyed(dearoreui::api::PageContextView const& view);
    void onFrame();

    static void nowParts(std::tm& out);
    [[nodiscard]] std::string seedEventsJson() const;

    dearoreui::api::ModId mModId;

    // Handles.
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
    std::optional<dearoreui::api::RegistrationHandle> mHostMethodHandle;
    std::optional<dearoreui::api::SubscriptionHandle> mReadySub;
    std::optional<dearoreui::api::SubscriptionHandle> mDestroyedSub;
    std::optional<dearoreui::api::SubscriptionHandle> mFrameSub;

    // Live data (client main thread only).
    std::optional<dearoreui::api::ContextId> mContextId;
    std::uint64_t                            mFrameCount{0};
    std::int64_t                             mLastClockSecond{-1};
};

} // namespace examples
} // namespace my_mod