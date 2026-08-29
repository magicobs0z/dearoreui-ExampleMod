#pragma once

#include "mod/examples/ExampleBase.h"

#include "mod/examples/PageScriptAsset.h"

#include "api/IDearOreUIApi.h"
#include "api/manifest/UiManifest.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"
#include "api/types/Id.h"

#include <optional>
#include <string_view>
#include <vector>

namespace my_mod {
namespace examples {

// Example 02 - Component Tree + Page Script.
//
// Teaches two things:
//   1. Declarative UI via ComponentSpec (ComponentKind + style + children).
//      The C++ side registers a minimal shell <div id="cal-root"> and a
//      <script> node that loads the page script.
//   2. Dynamic behavior via page script: the JS script builds the calendar
//      panel, draws the month grid, handles navigation (prev/next/today),
//      and manages click-to-select with today/selected highlighting.
//
// Layout conventions (see Docs/DearOreUI-布局规范化与引擎对齐-需求架构执行.md):
//   * Centered panel (position:fixed shell + explicit px panel)
//   * Grid built once, updated in-place (no DOM subtree rebuild)
//   * flex:1 distribution, margin-based spacing, no gap/grid/vw-vh
class Ex02Script final : public ExampleBase {
public:
    Ex02Script(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.script") {}

    ~Ex02Script() override = default;

    Ex02Script(Ex02Script const&)            = delete;
    Ex02Script& operator=(Ex02Script const&) = delete;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        // 1. Mod identity.
        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Calendar - Component + Script";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::UiMount,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.script] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        // 2. UI manifest.
        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_script";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = true;
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "script.v1";

        // 3. Component tree: fixed shell + page script.
        std::vector<dearoreui::api::DomNode> body;
        body.push_back(dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", "cal-root"}},
            .style = "position:fixed;top:0;left:0;right:0;bottom:0;overflow:hidden;",
            .text  = "",
        });
        std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex02_script.js");
        if (pageScript.empty()) {
            logger.error("[example.script] failed to load ex02_script.js");
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

        dearoreui::api::ComponentSpec root;
        root.kind = dearoreui::api::ComponentKind::Section;
        root.body = std::move(body);

        auto uiResult = mApi.registerComponent(mModId, uiManifest, root);
        if (uiResult.isErr()) {
            logger.error("[example.script] registerComponent failed: {}", uiResult.error().message);
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mUiHandle = uiResult.value();

        logger.info("[example.script] component + script registered");
        return true;
    }

    void shutdown() override {
        if (mUiHandle.has_value()) {
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
        }
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "02-component-script"; }

private:
    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
};

} // namespace examples
} // namespace my_mod