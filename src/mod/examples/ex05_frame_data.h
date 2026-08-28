#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"

#include "mod/examples/ExampleBase.h"
#include "mod/examples/PageScriptAsset.h"

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace my_mod {
namespace examples {

// Example 05 - Frame Drives Data (live clock bar).
//
// Teaches the periodic push source, on the calendar's clock bar:
//   * LL tick emitters are not ready during enable and there is no Level on
//     the main menu; ll coroutine executor execute()/executeAfter() did not
//     fire on the real client.
//   * The ONLY reliable cadence is the DearOreUI frame service:
//     subscribeFrame runs the callback once per client frame (game thread,
//     main menu included). This example pushes a live clock every 30 frames
//     (~0.5s).
//   * The frame callback runs on the game thread, so publishEvent (which
//     must be called from the game thread on this client) is safe directly.
//
// Success marker: the bottom clock bar ticks every second, and the today
// outline refreshes automatically across midnight (frame-driven).
class Ex05FrameData final : public ExampleBase {
public:
    Ex05FrameData(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.frame") {}

    ~Ex05FrameData() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Calendar Frame Example";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::UiMount,
            dearoreui::api::Permission::PageObserve,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.frame] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_clock";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = true;
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "calendar_clock.v1";

        std::vector<dearoreui::api::DomNode> body;
        body.push_back(dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", "cal-root"}},
            .style = "position:relative;width:280px;user-select:none;",
            .text  = "",
        });
        // T1: page script is an independent asset (<mod>/scripts/ex05_frame_data.js),
        // loaded at registration time and pushed through the verified <script>
        // injection channel. A missing asset aborts the UI registration.
        std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex05_frame_data.js");
        if (pageScript.empty()) {
            logger.error("[example.frame] failed to load page-script asset: ex05_frame_data.js");
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

        dearoreui::api::ComponentSpec panel;
        panel.kind  = dearoreui::api::ComponentKind::Panel;
        panel.style = "dark";
        panel.label = "Calendar - Frame Data";
        panel.body  = std::move(body);

        auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
        if (uiResult.isErr()) {
            logger.error("[example.frame] registerComponent failed: {}", uiResult.error().message);
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mUiHandle = uiResult.value();

        auto ready = mApi.subscribePage(
            dearoreui::api::PageSubscriptionOptions{mModId, {dearoreui::api::PageScope::Any}},
            dearoreui::api::PageEvent::Ready,
            [this](dearoreui::api::PageContextView const& view) { onReady(view); }
        );
        if (ready.isErr()) {
            logger.error("[example.frame] subscribePage(Ready) failed: {}", ready.error().message);
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mReadySub = ready.value();

        auto destroyed = mApi.subscribePage(
            dearoreui::api::PageSubscriptionOptions{mModId, {dearoreui::api::PageScope::Any}},
            dearoreui::api::PageEvent::Destroyed,
            [this](dearoreui::api::PageContextView const& view) { onDestroyed(view); }
        );
        if (destroyed.isErr()) {
            logger.error("[example.frame] subscribePage(Destroyed) failed: {}", destroyed.error().message);
            static_cast<void>(mApi.unsubscribePage(*mReadySub));
            mReadySub.reset();
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mDestroyedSub = destroyed.value();

        logger.info("[example.frame] registered (frame clock starts on page ready)");
        return true;
    }

    void shutdown() override {
        stopFrame();
        if (mDestroyedSub.has_value()) {
            static_cast<void>(mApi.unsubscribePage(*mDestroyedSub));
            mDestroyedSub.reset();
        }
        if (mReadySub.has_value()) {
            static_cast<void>(mApi.unsubscribePage(*mReadySub));
            mReadySub.reset();
        }
        if (mUiHandle.has_value()) {
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
        }
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "05-frame-data"; }

private:
    static void nowParts(int& y, int& mo, int& d, int& h, int& mi, int& s) {
        std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        y  = local.tm_year + 1900;
        mo = local.tm_mon + 1;
        d  = local.tm_mday;
        h  = local.tm_hour;
        mi = local.tm_min;
        s  = local.tm_sec;
    }

    void onReady(dearoreui::api::PageContextView const& view) {
        mContextId = view.id;
        if (!mFrameSub.has_value()) {
            auto handle = mApi.subscribeFrame(
                dearoreui::api::FrameSubscriptionOptions{mModId},
                [this]() { onFrame(); }
            );
            if (handle.isOk()) {
                mFrameSub = handle.value();
            } else {
                mMod.getSelf().getLogger().error("[example.frame] subscribeFrame failed: {}", handle.error().message);
            }
        }
    }

    void onDestroyed(dearoreui::api::PageContextView const&) {
        mContextId.reset();
        stopFrame();
    }

    void onFrame() {
        if (!mContextId.has_value()) return;
        ++mFrameCount;
        if (mFrameCount % 30 != 0) return; // ~0.5s at 60fps
        int y, mo, d, h, mi, s;
        nowParts(y, mo, d, h, mi, s);
        dearoreui::api::EventPublishOptions options;
        options.owner   = mModId;
        options.context = *mContextId;
        options.name    = "calendar.clock";
        options.payload = "{"
                          "\"y\":" + std::to_string(y) + ","
                          "\"m\":" + std::to_string(mo) + ","
                          "\"d\":" + std::to_string(d) + ","
                          "\"h\":" + std::to_string(h) + ","
                          "\"mi\":" + std::to_string(mi) + ","
                          "\"s\":" + std::to_string(s) + ""
                          "}";
        auto result = mApi.publishEvent(options);
        if (result.isErr()) {
            mMod.getSelf().getLogger().error("[example.frame] publish failed: {}", result.error().message);
        }
    }

    void stopFrame() {
        if (mFrameSub.has_value()) {
            static_cast<void>(mApi.unsubscribeFrame(*mFrameSub));
            mFrameSub.reset();
        }
    }

    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
    std::optional<dearoreui::api::SubscriptionHandle> mReadySub;
    std::optional<dearoreui::api::SubscriptionHandle> mDestroyedSub;
    std::optional<dearoreui::api::SubscriptionHandle> mFrameSub;
    std::optional<dearoreui::api::ContextId>          mContextId;
    std::uint64_t                                     mFrameCount{0};
};

} // namespace examples
} // namespace my_mod