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

// Example 03 - Communication Channels.
//
// Demonstrates the three communication mechanisms between C++ and the page:
//   1. C++ -> JS: publishEvent pushes seed events (green dots) on page ready.
//   2. C++ -> JS: frame-driven clock tick (calendar.clock) via subscribeFrame.
//   3. JS -> C++: registered host method "calendar.init" (facet single-dispatch).
//
// The page script (ex03_communication.js) builds the same centered panel
// calendar as ex02, but adds event dots, a live clock, and calls the host
// method to fetch authoritative data.
//
// Engine constraints honored:
//   * Page script does NOT dispatch JS->C++ (the facet budget is kept for
//     the calendar.init demonstration; the page calls it once).
//   * Frame-driven pushes run on the DearOreUI client-frame service.
class Ex03Communication final : public ExampleBase {
public:
    Ex03Communication(dearoreui::api::IDearOreUIApi& api, MyMod& mod);
    ~Ex03Communication() override;

    Ex03Communication(Ex03Communication const&)            = delete;
    Ex03Communication& operator=(Ex03Communication const&) = delete;

    [[nodiscard]] bool registerAll() override;
    void shutdown() override;

    [[nodiscard]] std::string_view name() const override { return "03-communication"; }

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