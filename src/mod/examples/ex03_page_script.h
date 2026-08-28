#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"

#include "mod/examples/ExampleBase.h"
#include "mod/examples/PageScriptAsset.h"

#include <optional>
#include <string_view>
#include <vector>

namespace my_mod {
namespace examples {

// Example 03 - Page Script & Interaction (live month grid).
//
// The calendar's real UI is drawn by the page script. LESSON FROM THE FIELD:
// the engine's page context does NOT honor CSS Grid/Flex reliably (the first
// calendar attempt rendered days scattered across the screen). This kernel
// therefore uses the most basic primitive the engine demonstrably applies -
// absolute positioning with explicit pixel coordinates - styled to match the
// vanilla dark component look (deep background, thin border, rounded corners,
// blue accent for today/selection). Interactive pieces are plain DOM divs
// wired with addEventListener (component-state clicks are not reliably
// synthesized on this cohtml build).
//
// The page script channel itself: only the C++ injector (ExecuteScript right
// after mount, via a <script> DomNode in the ComponentSpec body) executes
// page JS; DOM <script> nodes and eval() crash the engine (0x40080201).
//
// Success markers: correct 7x6 grid alignment, today outlined, prev/next
// paging, click-to-select.
class Ex03PageScript final : public ExampleBase {
public:
    Ex03PageScript(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.script") {}

    ~Ex03PageScript() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Calendar Grid Example";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::UiMount,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.script] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_grid";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = true; // prev/next/today and day clicks
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "calendar_grid.v1";

        // Body: one root container (the page script positions all children
        // absolutely with explicit pixels) + the injected <script>.
        std::vector<dearoreui::api::DomNode> body;
        body.push_back(dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", "cal-root"}},
            .style = "position:relative;width:280px;user-select:none;",
            .text  = "",
        });
        // T1: page script is an independent asset (<mod>/scripts/ex03_page_script.js),
        // loaded at registration time and pushed through the verified <script>
        // injection channel. A missing asset aborts the UI registration.
        std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex03_page_script.js");
        if (pageScript.empty()) {
            logger.error("[example.script] failed to load page-script asset: ex03_page_script.js");
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

        dearoreui::api::ComponentSpec panel;
        panel.kind  = dearoreui::api::ComponentKind::Panel;
        panel.style = "dark";
        panel.label = "Calendar";
        panel.body  = std::move(body);

        auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
        if (uiResult.isErr()) {
            logger.error("[example.script] registerComponent failed: {}", uiResult.error().message);
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mUiHandle = uiResult.value();
        logger.info("[example.script] calendar grid registered (absolute-positioned page script)");
        return true;
    }

    void shutdown() override {
        if (mUiHandle.has_value()) {
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
        }
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "03-page-script"; }

private:

    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
};

} // namespace examples
} // namespace my_mod