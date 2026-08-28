#include "mod/examples/ex07_calendar.h"

#include "mod/MyMod.h"

#include "mod/examples/PageScriptAsset.h"

#include "api/IHostMethod.h"

#include "api/manifest/UiManifest.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"
#include "api/types/Page.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace my_mod {
namespace examples {

namespace {

// ---------------------------------------------------------------------------
// Page script. T1 asset-ized: the content lives in assets/scripts/ex07_calendar.js,
// shipped as <mod>/scripts/ex07_calendar.js and loaded by loadPageScriptAsset at
// registration time (see registerAll). It is pushed as a <script> DomNode through
// the verified injection channel (DOM <script> nodes / eval() crash the engine).
// The script never builds UI; it caches refs to the component-tree nodes, sizes
// flexible regions once (geometry, resize-only), fills data with a DOM-diff
// render, and delegates clicks.
// ---------------------------------------------------------------------------

// Server side of the registered single-dispatch capability. Kept as a
// demonstration (the page script does not invoke it; all data flows C++->JS).
class CalendarInitMethod final : public dearoreui::api::IHostMethod {
public:
    explicit CalendarInitMethod(CalendarExample& owner) : mOwner(owner) {}
    [[nodiscard]] std::string name() const override { return "calendar.init"; }
    [[nodiscard]] dearoreui::api::Permission requiredPermission() const override {
        return dearoreui::api::Permission::HostReadOnly;
    }
    [[nodiscard]] dearoreui::api::Result<std::string>
    execute(dearoreui::api::ContextId /*contextId*/, std::string_view /*args*/) override {
        return dearoreui::api::Result<std::string>::success(mOwner.handleInit());
    }
    CalendarExample& mOwner;
};

} // namespace

// ---------------------------------------------------------------------------
// CalendarExample
// ---------------------------------------------------------------------------

CalendarExample::CalendarExample(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
: ExampleBase(api, mod),
  mModId("example.calendar") {}

CalendarExample::~CalendarExample() { shutdown(); }

void CalendarExample::nowParts(std::tm& out) {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
}

// Seed events for a few days around today (pushed to the page on Ready).
std::string CalendarExample::seedEventsJson() const {
    std::tm t{};
    nowParts(t);
    char key[16];
    std::string json = "{\"events\":{";
    const char* anchorTexts[3][2] = {
        {"今日计划", "示例事件"},
        {"午休 12:00", "提交周报"},
        {"周末活动", nullptr},
    };
    for (int off = 0; off < 3; ++off) {
        std::tm day = t;
        day.tm_mday += off;
        std::mktime(&day);
        std::snprintf(key, sizeof(key), "%04d-%02d-%02d", day.tm_year + 1900, day.tm_mon + 1, day.tm_mday);
        if (off > 0) json += ",";
        json += "\"" + std::string(key) + "\":[";
        int first = true;
        for (int i = 0; i < 2 && anchorTexts[off][i] != nullptr; ++i) {
            if (!first) json += ",";
            first = false;
            json += "\"" + std::string(anchorTexts[off][i]) + "\"";
        }
        json += "]";
    }
    json += "}}";
    return json;
}

std::string CalendarExample::handleInit() const { return seedEventsJson(); }

bool CalendarExample::registerAll() {
    auto& logger = mMod.getSelf().getLogger();

    // 0. Mod identity.
    dearoreui::api::ModManifest modManifest;
    modManifest.id           = mModId;
    modManifest.modNamespace = mModId.value();
    modManifest.displayName  = "Calendar Example Mod";
    modManifest.modVersion   = dearoreui::api::Version{1, 0, 0};
    modManifest.permissions  = {
        dearoreui::api::Permission::HostReadOnly,
        dearoreui::api::Permission::PageObserve,
        dearoreui::api::Permission::UiMount,
    };
    auto modRegistered = mApi.registerMod(modManifest);
    if (modRegistered.isErr()) {
        logger.error("[example.calendar] registerMod failed: {}", modRegistered.error().message);
        return false;
    }

    // 1. UI.
    dearoreui::api::UiManifest uiManifest;
    uiManifest.modNamespace  = mModId.value();
    uiManifest.id            = "calendar";
    uiManifest.kind          = dearoreui::api::UiKind::Overlay;
    uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
    uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
    uiManifest.pointerEvents = true;
    uiManifest.containerId =
        dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
    uiManifest.fingerprint = "calendar.v1";

    // R1/R2/R3: the whole static skeleton is DECLARED here - the component
    // tree is the single source of layout. Flexbox-only (the only legal
    // display values on the engine are flex/none); the only absolute
    // positioning is the fixed full-screen root. The page script never builds
    // UI - it caches refs, does resize-only geometry and DOM-diff data fills.
    auto navButton = [](char const* id, char const* text) {
        return dearoreui::api::DomNode{
            .tag   = "div",
            .attrs = {{"id", id}},
            .style = "flex:none;padding:6px 16px;background:#21262d;"
                     "border:1px solid #30363d;border-radius:6px;color:#ffffff;"
                     "font-size:14px;cursor:pointer;",
            .text = text,
        };
    };

    // Grid = 6 flex rows x 7 flex cells (data-index 0..41, DOM row-major
    // order - the page script walks cells[j] in that same order).
    std::vector<dearoreui::api::DomNode> gridRows;
    for (int row = 0; row < 6; ++row) {
        std::vector<dearoreui::api::DomNode> cells;
        for (int col = 0; col < 7; ++col) {
            cells.push_back(dearoreui::api::DomNode{
                .tag   = "div",
                .attrs = {{"data-index", std::to_string(row * 7 + col)}},
                .style = "flex:1;position:relative;display:flex;align-items:center;"
                         "justify-content:center;border:1px solid transparent;"
                         "border-radius:6px;font-size:16px;cursor:pointer;",
            });
        }
        gridRows.push_back(dearoreui::api::DomNode{
            .tag      = "div",
            .attrs    = {{"data-gridrow", ""}},
            .style    = "flex:1;display:flex;flex-direction:row;gap:4px;",
            .children = std::move(cells),
        });
    }

    std::vector<dearoreui::api::DomNode> body;
    body.push_back(dearoreui::api::DomNode{
        .tag   = "div",
        .attrs = {{"id", "cal-root"}},
        // Verified full-screen pattern (stage 7.1): inset 0 instead of the
        // unverified 100vw/100vh viewport units. Flex column distributes the
        // header / grid / bottom regions; no per-pixel coordinates anywhere.
        .style = "position:fixed;top:0;left:0;right:0;bottom:0;overflow:hidden;"
                 "display:flex;flex-direction:column;background:rgba(0,0,0,.95);",
        .children = {
            // Header: left spacer + centered title + right nav (flex equalizes
            // the two spacers so the title stays centered).
            dearoreui::api::DomNode{
                .tag   = "div",
                .attrs = {{"id", "cal-header"}},
                .style = "flex:none;display:flex;align-items:center;height:60px;padding:0 24px;",
                .children = {
                    dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-left"}}, .style = "flex:1;"},
                    dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-title"}},
                                            .style = "flex:none;font-size:22px;color:#ffffff;font-weight:600;letter-spacing:2px;"},
                    dearoreui::api::DomNode{
                        .tag   = "div",
                        .attrs = {{"id", "cal-nav"}},
                        .style = "flex:1;display:flex;align-items:center;justify-content:flex-end;gap:10px;",
                        .children = {
                            navButton("cal-prev", "◀"),
                            navButton("cal-today", "今天"),
                            navButton("cal-next", "▶"),
                        },
                    },
                },
            },
            // Grid: 6 flex rows x 7 flex cells.
            dearoreui::api::DomNode{
                .tag      = "div",
                .attrs    = {{"id", "cal-grid"}},
                .style    = "flex:1;display:flex;flex-direction:column;padding:0 24px;gap:4px;",
                .children = std::move(gridRows),
            },
            // Bottom: date detail + event list + input row + clock.
            dearoreui::api::DomNode{
                .tag   = "div",
                .attrs = {{"id", "cal-bottom"}},
                .style = "flex:none;display:flex;flex-direction:column;padding:0 24px 24px;",
                .children = {
                    dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-day"}},
                                            .style = "flex:none;height:34px;line-height:34px;font-size:16px;color:#d0d7de;"},
                    dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-list"}},
                                            .style = "flex:1;overflow:hidden;margin-top:4px;"},
                    dearoreui::api::DomNode{
                        .tag   = "div",
                        .attrs = {{"id", "cal-actionrow"}},
                        .style = "flex:none;display:flex;gap:10px;margin-top:12px;",
                        .children = {
                            dearoreui::api::DomNode{.tag = "input", .attrs = {{"id", "cal-input"}},
                                                    .style = "flex:1;height:40px;padding:0 12px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#ffffff;font-size:14px;outline:none;"},
                            dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-add"}},
                                                    .style = "flex:none;width:64px;height:40px;background:#238636;border:1px solid #2ea043;border-radius:6px;color:#ffffff;font-size:14px;cursor:pointer;display:flex;align-items:center;justify-content:center;",
                                                    .text = "添加"},
                        },
                    },
                    dearoreui::api::DomNode{.tag = "div", .attrs = {{"id", "cal-clock"}},
                                            .style = "flex:none;height:30px;line-height:30px;margin-top:8px;text-align:center;color:#3fb950;font-size:16px;letter-spacing:2px;"},
                },
            },
        },
    });
    // T1: the page script is an independent asset (<mod>/scripts/ex07_calendar.js),
    // loaded at registration time and pushed through the verified <script>
    // injection channel (DOM <script src> nodes are not executable on the
    // engine). A missing asset aborts the UI registration - never inject a
    // blank script.
    std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex07_calendar.js");
    if (pageScript.empty()) {
        logger.error("[example.calendar] failed to load page-script asset: ex07_calendar.js");
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

    dearoreui::api::ComponentSpec root;
    root.kind = dearoreui::api::ComponentKind::Section;
    root.body = std::move(body);

    dearoreui::api::ComponentSpec const& panel = root;

    auto uiResult = mApi.registerComponent(mModId, uiManifest, panel);
    if (uiResult.isErr()) {
        logger.error("[example.calendar] registerComponent failed: {}", uiResult.error().message);
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    mUiHandle = uiResult.value();

    // 2. Host method (single-dispatch capability, registered for
    // demonstration; the page script does not invoke it).
    dearoreui::api::HostMethodManifest hostManifest;
    hostManifest.name        = "calendar.init";
    hostManifest.pageScopes  = {dearoreui::api::PageScope::Any};
    hostManifest.permissions = dearoreui::api::PermissionSet{
        std::vector{dearoreui::api::Permission::HostReadOnly}
    };
    auto host = mApi.registerHostMethod(
        mModId,
        hostManifest,
        std::make_shared<CalendarInitMethod>(*this)
    );
    if (host.isErr()) {
        logger.error("[example.calendar] registerHostMethod failed: {}", host.error().message);
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    mHostMethodHandle = host.value();

    // 3. Page lifecycle.
    auto ready = mApi.subscribePage(
        dearoreui::api::PageSubscriptionOptions{mModId, {dearoreui::api::PageScope::Any}},
        dearoreui::api::PageEvent::Ready,
        [this](dearoreui::api::PageContextView const& view) { onPageReady(view); }
    );
    if (ready.isErr()) {
        logger.error("[example.calendar] subscribePage(Ready) failed: {}", ready.error().message);
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    mReadySub = ready.value();

    auto destroyed = mApi.subscribePage(
        dearoreui::api::PageSubscriptionOptions{mModId, {dearoreui::api::PageScope::Any}},
        dearoreui::api::PageEvent::Destroyed,
        [this](dearoreui::api::PageContextView const& view) { onPageDestroyed(view); }
    );
    if (destroyed.isErr()) {
        logger.error("[example.calendar] subscribePage(Destroyed) failed: {}", destroyed.error().message);
        static_cast<void>(mApi.unsubscribePage(*mReadySub));
        mReadySub.reset();
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    mDestroyedSub = destroyed.value();

    logger.info("[example.calendar] full calendar registered");
    return true;
}

void CalendarExample::shutdown() {
    if (mFrameSub.has_value()) {
        static_cast<void>(mApi.unsubscribeFrame(*mFrameSub));
        mFrameSub.reset();
    }
    if (mDestroyedSub.has_value()) {
        static_cast<void>(mApi.unsubscribePage(*mDestroyedSub));
        mDestroyedSub.reset();
    }
    if (mReadySub.has_value()) {
        static_cast<void>(mApi.unsubscribePage(*mReadySub));
        mReadySub.reset();
    }
    if (mHostMethodHandle.has_value()) {
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
    }
    if (mUiHandle.has_value()) {
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
    }
    mContextId.reset();
    static_cast<void>(mApi.unregisterMod(mModId));
}

void CalendarExample::onPageReady(dearoreui::api::PageContextView const& view) {
    mContextId = view.id;

    // Seed events, C++ -> JS (single bulk push; the page keeps its dispatch
    // budget unused - see class comment).
    dearoreui::api::EventPublishOptions seed;
    seed.owner   = mModId;
    seed.context = view.id;
    seed.name    = "calendar.events";
    seed.payload = seedEventsJson();
    auto seedResult = mApi.publishEvent(seed);
    if (seedResult.isErr()) {
        mMod.getSelf().getLogger().error("[example.calendar] publish events failed: {}", seedResult.error().message);
    }

    // Frame-driven clock.
    if (!mFrameSub.has_value()) {
        auto handle = mApi.subscribeFrame(
            dearoreui::api::FrameSubscriptionOptions{mModId},
            [this]() { onFrame(); }
        );
        if (handle.isOk()) {
            mFrameSub = handle.value();
        } else {
            mMod.getSelf().getLogger().error("[example.calendar] subscribeFrame failed: {}", handle.error().message);
        }
    }
}

void CalendarExample::onPageDestroyed(dearoreui::api::PageContextView const&) {
    mContextId.reset();
    if (mFrameSub.has_value()) {
        static_cast<void>(mApi.unsubscribeFrame(*mFrameSub));
        mFrameSub.reset();
    }
    mLastClockSecond = -1;
}

void CalendarExample::onFrame() {
    if (!mContextId.has_value()) return;
    ++mFrameCount;
    if (mFrameCount % 30 != 0) return; // ~0.5s cadence

    std::tm t{};
    nowParts(t);
    std::int64_t sec = static_cast<std::int64_t>(t.tm_hour) * 3600 + static_cast<std::int64_t>(t.tm_min) * 60 + t.tm_sec;
    if (sec == mLastClockSecond) return; // dedupe within the same second
    mLastClockSecond = sec;

    dearoreui::api::EventPublishOptions clock;
    clock.owner   = mModId;
    clock.context = *mContextId;
    clock.name    = "calendar.clock";
    clock.payload = "{"
                    "\"y\":" + std::to_string(t.tm_year + 1900) + ","
                    "\"m\":" + std::to_string(t.tm_mon + 1) + ","
                    "\"d\":" + std::to_string(t.tm_mday) + ","
                    "\"h\":" + std::to_string(t.tm_hour) + ","
                    "\"mi\":" + std::to_string(t.tm_min) + ","
                    "\"s\":" + std::to_string(t.tm_sec) + ""
                    "}";
    auto result = mApi.publishEvent(clock);
    if (result.isErr()) {
        mMod.getSelf().getLogger().error("[example.calendar] publish clock failed: {}", result.error().message);
    }
}

} // namespace examples
} // namespace my_mod