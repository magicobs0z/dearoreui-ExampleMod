#pragma once

#include "mod/examples/ExampleBase.h"

#include "api/IDearOreUIApi.h"
#include "api/IHostMethod.h"
#include "api/types/Event.h"
#include "api/types/Id.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace my_mod {
namespace examples {

// Example 04 - Calendar Demo (graduation project).
//
// The polished, full-featured calendar that serves as the official reference
// implementation. Combines everything from lessons 01-03:
//   * Centered panel calendar (modern design, follows layout conventions)
//   * Month grid with paging / today highlight / click-to-select
//   * C++->JS pushes: seed events on page ready + frame-driven clock
//   * Event dots + detail list with local add/delete (JS-side store)
//   * Registered host method "calendar.init" (facet single-dispatch)
//
// The page script (ex04_calendar.js) is the most polished version with
// visual refinements, smooth transitions, and event management UI.
class Ex04Calendar final : public ExampleBase {
public:
    Ex04Calendar(dearoreui::api::IDearOreUIApi& api, MyMod& mod);
    ~Ex04Calendar() override;

    Ex04Calendar(Ex04Calendar const&)            = delete;
    Ex04Calendar& operator=(Ex04Calendar const&) = delete;

    [[nodiscard]] bool registerAll() override;
    void shutdown() override;

    [[nodiscard]] std::string_view name() const override { return "04-calendar-demo"; }

    /// Host method handler: returns the aggregated session snapshot.
    [[nodiscard]] std::string handleInit() const;

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

    // Live data.
    std::optional<dearoreui::api::ContextId> mContextId;
    std::uint64_t                            mFrameCount{0};
    std::int64_t                             mLastClockSecond{-1};
};

} // namespace examples
} // namespace my_mod