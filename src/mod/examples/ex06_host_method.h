#pragma once

#include "api/IDearOreUIApi.h"
#include "api/IHostMethod.h"
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

// Example 06 - Host Method (JS -> C++), default OFF.
//
// Teaches the JS->C++ channel on the calendar: the page script calls
//   window.oreui.host.call("calendar.init", {...})
// which the platform routes through the native "dearoreui" facet. The host
// answers with a batch SEED-EVENT snapshot (today + tomorrow), which the page
// renders as dots on the grid - a full round trip in one dispatch.
//
// IMPORTANT engine constraints (verified on the real client):
//   * The RegisterForEvent/BindCall binding channel CRASHES this client (the
//     game raises its own unhandled msxml6 error, 0x40080201) - mods cannot
//     use it. The facet channel is the healthy JS->C++ path.
//   * After dispatching init, the JS heartbeat on the card should KEEP
//     ticking - proving the page script survives the facet call. If it stops
//     on your build, report it (this example exists to re-verify that).
//   * Only ONE dispatch slot exists per view (ViewDispatchAlreadyUsed after).
//     Use it for a batch initial snapshot, like this example.
//
// The example is DISABLED by default (only reachable via config "06") - it is
// a capability demonstration, not part of the default 07 flow (07 pushes its
// seed events C++->JS instead, so the page keeps its single dispatch budget
// unused and its JS side stays simple).
class Ex06HostMethod final : public ExampleBase {
public:
    Ex06HostMethod(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.host") {}

    ~Ex06HostMethod() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Calendar Host Method Example";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::HostReadOnly,
            dearoreui::api::Permission::UiMount,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.host] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        dearoreui::api::HostMethodManifest hostManifest;
        hostManifest.name        = "calendar.init";
        hostManifest.pageScopes  = {dearoreui::api::PageScope::Any};
        hostManifest.permissions = dearoreui::api::PermissionSet{
            std::vector{dearoreui::api::Permission::HostReadOnly}
        };
        auto host = mApi.registerHostMethod(
            mModId,
            hostManifest,
            std::make_shared<InitMethod>(*this)
        );
        if (host.isErr()) {
            logger.error("[example.host] registerHostMethod failed: {}", host.error().message);
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mHostHandle = host.value();

        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_init";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = true;
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "calendar_init.v1";

        std::vector<dearoreui::api::DomNode> body;
        body.push_back(dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", "cal-root"}},
            .style = "position:relative;width:280px;user-select:none;",
            .text  = "",
        });
        // T1: page script is an independent asset (<mod>/scripts/ex06_host_method.js),
        // loaded at registration time and pushed through the verified <script>
        // injection channel. A missing asset aborts the UI registration.
        std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex06_host_method.js");
        if (pageScript.empty()) {
            logger.error("[example.host] failed to load page-script asset: ex06_host_method.js");
            static_cast<void>(mApi.unregisterHostMethod(*mHostHandle));
            mHostHandle.reset();
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

        dearoreui::api::ComponentSpec panel;
        panel.kind  = dearoreui::api::ComponentKind::Panel;
        panel.style = "dark";
        panel.label = "Calendar - Host Method";
        panel.body  = std::move(body);

        auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
        if (uiResult.isErr()) {
            logger.error("[example.host] registerComponent failed: {}", uiResult.error().message);
            static_cast<void>(mApi.unregisterHostMethod(*mHostHandle));
            mHostHandle.reset();
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mUiHandle = uiResult.value();
        logger.info("[example.host] registered; page dispatches calendar.init on boot");
        return true;
    }

    void shutdown() override {
        if (mUiHandle.has_value()) {
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
        }
        if (mHostHandle.has_value()) {
            static_cast<void>(mApi.unregisterHostMethod(*mHostHandle));
            mHostHandle.reset();
        }
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "06-host-method"; }

    // Batch seed-event snapshot, keyed by local dates (today + tomorrow).
    [[nodiscard]] std::string handleInit() const {
        std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        char today[12];
        std::snprintf(today, sizeof(today), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
        std::tm tomorrow = local;
        ++tomorrow.tm_mday;
        std::mktime(&tomorrow);
        char next[12];
        std::snprintf(next, sizeof(next), "%04d-%02d-%02d", tomorrow.tm_year + 1900, tomorrow.tm_mon + 1, tomorrow.tm_mday);
        return "{"
               "\"events\":{"
               "\"" + std::string(today) + "\":[\"demo 09:00\",\"host pushed\"],"
               "\"" + std::string(next) + "\":[\"tomorrow plan\"]"
               "}}";
    }

private:
    class InitMethod final : public dearoreui::api::IHostMethod {
    public:
        explicit InitMethod(Ex06HostMethod& owner) : mOwner(owner) {}
        [[nodiscard]] std::string name() const override { return "calendar.init"; }
        [[nodiscard]] dearoreui::api::Permission requiredPermission() const override {
            return dearoreui::api::Permission::HostReadOnly;
        }
        [[nodiscard]] dearoreui::api::Result<std::string>
        execute(dearoreui::api::ContextId /*contextId*/, std::string_view /*args*/) override {
            return dearoreui::api::Result<std::string>::success(mOwner.handleInit());
        }
        Ex06HostMethod& mOwner;
    };

    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mHostHandle;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
};

} // namespace examples
} // namespace my_mod