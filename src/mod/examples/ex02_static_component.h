#pragma once

#include "mod/examples/ExampleBase.h"

#include <optional>
#include <string_view>

namespace my_mod {
namespace examples {

// Example 02 - Static Component (calendar skeleton).
//
// Teaches the declarative component pipeline: instead of hand-written HTML,
// the mod describes a UI with ComponentSpec tree (ComponentKind + style +
// label + children), which DearOreUI renders with the VANILLA component
// library. This lesson draws the STATIC SKELETON of the calendar we will
// build across the whole 01-07 course: panel + month caption + weekday
// header row + a hint that the live grid arrives in lesson 03.
//
// Rules learned the hard way on the real client:
//   * Static labels MUST stay ASCII - the vanilla renderer fonts (Minecraft
//     Ten/Seven/Five) have no CJK glyphs (Chinese renders as broken boxes).
//     Dynamic text written by the page script uses Noto Sans and is fine.
//   * pageScopes = {PageScope::Any} is the correct wildcard (fixed platform
//     bug: previously Any never matched concrete pages).
//   * uiManifest.containerId should be built with makeUiContainerId.
class Ex02StaticComponent final : public ExampleBase {
public:
    Ex02StaticComponent(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.component") {}

    ~Ex02StaticComponent() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest modManifest;
        modManifest.id           = mModId;
        modManifest.modNamespace = mModId.value();
        modManifest.displayName  = "Static Calendar Skeleton";
        modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        modManifest.permissions  = {
            dearoreui::api::Permission::UiMount,
        };
        auto modRegistered = mApi.registerMod(modManifest);
        if (modRegistered.isErr()) {
            logger.error("[example.component] registerMod failed: {}", modRegistered.error().message);
            return false;
        }

        dearoreui::api::UiManifest uiManifest;
        uiManifest.modNamespace  = mModId.value();
        uiManifest.id            = "calendar_skeleton";
        uiManifest.kind          = dearoreui::api::UiKind::Overlay;
        uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
        uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
        uiManifest.pointerEvents = false; // static display only
        uiManifest.containerId =
            dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
        uiManifest.fingerprint = "calendar_skeleton.v1";

        // Declarative tree: Panel(dark) -> Card containing the calendar
        // skeleton. Layout primitives beyond Panel/Card/Text are NOT relied
        // upon here (the engine's Grid/flex in the component renderer is not
        // verified); the weekday strip is a single ASCII Text line.
        dearoreui::api::ComponentSpec caption;
        caption.kind  = dearoreui::api::ComponentKind::Text;
        caption.label = "AUGUST 2026 (static skeleton)";

        dearoreui::api::ComponentSpec weekdayRow;
        weekdayRow.kind  = dearoreui::api::ComponentKind::Text;
        weekdayRow.label = "SUN   MON   TUE   WED   THU   FRI   SAT";

        dearoreui::api::ComponentSpec hint;
        hint.kind  = dearoreui::api::ComponentKind::Text;
        hint.label = "Lesson 03 draws the live month grid via page script.";

        dearoreui::api::ComponentSpec card;
        card.kind     = dearoreui::api::ComponentKind::Card;
        card.style    = "bordered";
        card.children = {std::move(caption), std::move(weekdayRow), std::move(hint)};

        dearoreui::api::ComponentSpec panel;
        panel.kind     = dearoreui::api::ComponentKind::Panel;
        panel.style    = "dark";
        panel.label    = "Calendar - Static Component";
        panel.children = {std::move(card)};

        auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
        if (uiResult.isErr()) {
            logger.error("[example.component] registerComponent failed: {}", uiResult.error().message);
            static_cast<void>(mApi.unregisterMod(mModId));
            return false;
        }
        mUiHandle = uiResult.value();
        logger.info("[example.component] calendar skeleton registered (ascii labels only)");
        return true;
    }

    void shutdown() override {
        if (mUiHandle.has_value()) {
            static_cast<void>(mApi.unregisterUi(*mUiHandle));
            mUiHandle.reset();
        }
        // unregisterMod is idempotent and keyed by ModId.
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "02-static-component"; }

private:
    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
};

} // namespace examples
} // namespace my_mod