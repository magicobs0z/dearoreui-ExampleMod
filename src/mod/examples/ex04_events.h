#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"

#include "mod/examples/ExampleBase.h"
#include "mod/examples/PageScriptAsset.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ctime>

namespace my_mod {
namespace examples {

// Example 04 - Events (C++ -> JS): today pushed from the host.
//
// Teaches the event push contract on the calendar:
//   * subscribePage(PageEvent::Ready, scopes={Any}) fires once the page is
//     mounted and its script context is ready.
//   * publishEvent(EventPublishOptions) serializes an event into the page:
//     JS receives it through window.oreui.event.on(name, cb) - the callback
//     gets the payload object directly (no JSON.parse).
//   * The host pushes a one-shot "calendar.today" snapshot on page ready;
//     the page initializes its grid from the pushed date (falling back to
//     the local clock if no push arrives yet).
//
// Success marker: month caption and today outline come from the C++ payload;
// the info line shows the push count ("push: 1 (from C++)").
class Ex04Events final : public ExampleBase {
public:
    Ex04Events(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.events") {}

    ~Ex04Events() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Calendar Events Example";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::UiMount,
            dearoreui::api::Permission::PageObserve,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.events] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_events";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = true;
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "calendar_events.v1";

        std::vector<dearoreui::api::DomNode> body;
        body.push_back(dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", "cal-root"}},
            .style = "position:relative;width:280px;user-select:none;",
            .text  = "",
        });
        // T1: page script is an independent asset (<mod>/scripts/ex04_events.js),
        // loaded at registration time and pushed through the verified <script>
        // injection channel. A missing asset aborts the UI registration.
        std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex04_events.js");
        if (pageScript.empty()) {
            logger.error("[example.events] failed to load page-script asset: ex04_events.js");
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

        dearoreui::api::ComponentSpec panel;
        panel.kind  = dearoreui::api::ComponentKind::Panel;
        panel.style = "dark";
        panel.label = "Calendar - Events";
        panel.body  = std::move(body);

        auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
        if (uiResult.isErr()) {
            logger.error("[example.events] registerComponent failed: {}", uiResult.error().message);
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
            logger.error("[example.events] subscribePage(Ready) failed: {}", ready.error().message);
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mReadySub = ready.value();
        logger.info("[example.events] registered (pushes today on page ready)");
        return true;
    }

    void shutdown() override {
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

    [[nodiscard]] std::string_view name() const override { return "04-events"; }

private:
    void onReady(dearoreui::api::PageContextView const& view) {
        mPushes++;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        dearoreui::api::EventPublishOptions options;
        options.owner   = mModId;
        options.context = view.id;
        options.name    = "calendar.today";
        options.payload = "{"
                          "\"y\":" + std::to_string(local.tm_year + 1900) + ","
                          "\"m\":" + std::to_string(local.tm_mon + 1) + ","
                          "\"d\":" + std::to_string(local.tm_mday) + ","
                          "\"pushes\":" + std::to_string(mPushes) + ""
                          "}";
        auto result = mApi.publishEvent(options);
        if (result.isErr()) {
            mMod.getSelf().getLogger().error("[example.events] publish failed: {}", result.error().message);
        }
    }

    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
    std::optional<dearoreui::api::SubscriptionHandle> mReadySub;
    std::uint64_t                                     mPushes{0};
};

} // namespace examples
} // namespace my_mod