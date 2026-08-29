#include "mod/examples/ex04_calendar.h"

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

// Host method: returns the session snapshot (today, events, config).
class CalendarInitMethod final : public dearoreui::api::IHostMethod {
public:
    explicit CalendarInitMethod(Ex04Calendar& owner) : mOwner(owner) {}
    [[nodiscard]] std::string name() const override { return "calendar.init"; }
    [[nodiscard]] dearoreui::api::Permission requiredPermission() const override {
        return dearoreui::api::Permission::HostReadOnly;
    }
    [[nodiscard]] dearoreui::api::Result<std::string>
    execute(dearoreui::api::ContextId /*contextId*/, std::string_view /*args*/) override {
        return dearoreui::api::Result<std::string>::success(mOwner.handleInit());
    }
    Ex04Calendar& mOwner;
};

} // namespace

Ex04Calendar::Ex04Calendar(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
: ExampleBase(api, mod),
  mModId("example.calendar") {}

Ex04Calendar::~Ex04Calendar() { shutdown(); }

void Ex04Calendar::nowParts(std::tm& out) {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
}

std::string Ex04Calendar::seedEventsJson() const {
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

std::string Ex04Calendar::handleInit() const { return seedEventsJson(); }

bool Ex04Calendar::registerAll() {
    auto& logger = mMod.getSelf().getLogger();

    // 0. Mod identity.
    dearoreui::api::ModManifest modManifest;
    modManifest.id           = mModId;
    modManifest.modNamespace = mModId.value();
    modManifest.displayName  = "Calendar Demo";
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
    uiManifest.id            = "calendar_demo";
    uiManifest.kind          = dearoreui::api::UiKind::Overlay;
    uiManifest.pageScopes    = {dearoreui::api::PageScope::Any};
    uiManifest.anchor        = dearoreui::api::UiAnchor::TopRight;
    uiManifest.pointerEvents = true;
    uiManifest.containerId =
        dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
    uiManifest.fingerprint = "demo.v1";

    std::vector<dearoreui::api::DomNode> body;
    body.push_back(dearoreui::api::DomNode{
        .tag   = "div",
        .attrs = {{"id", "cal-root"}},
        .style = "position:fixed;top:0;left:0;right:0;bottom:0;overflow:hidden;",
        .text  = "",
    });
    std::string pageScript = loadPageScriptAsset(mMod.getSelf().getModDir(), "ex04_calendar.js");
    if (pageScript.empty()) {
        logger.error("[example.calendar] failed to load ex04_calendar.js");
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    body.push_back(dearoreui::api::DomNode{.tag = "script", .text = std::move(pageScript)});

    dearoreui::api::ComponentSpec root;
    root.kind = dearoreui::api::ComponentKind::Section;
    root.body = std::move(body);

    auto uiResult = mApi.registerComponent(mModId, uiManifest, root);
    if (uiResult.isErr()) {
        logger.error("[example.calendar] registerComponent failed: {}", uiResult.error().message);
        static_cast<void>(mApi.unregisterMod(mModId));
        return false;
    }
    mUiHandle = uiResult.value();

    // 2. Host method.
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

    logger.info("[example.calendar] calendar demo registered");
    return true;
}

void Ex04Calendar::shutdown() {
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

void Ex04Calendar::onPageReady(dearoreui::api::PageContextView const& view) {
    mContextId = view.id;

    dearoreui::api::EventPublishOptions seed;
    seed.owner   = mModId;
    seed.context = view.id;
    seed.name    = "calendar.events";
    seed.payload = seedEventsJson();
    auto seedResult = mApi.publishEvent(seed);
    if (seedResult.isErr()) {
        mMod.getSelf().getLogger().error("[example.calendar] publish events failed: {}", seedResult.error().message);
    }

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

void Ex04Calendar::onPageDestroyed(dearoreui::api::PageContextView const&) {
    mContextId.reset();
    if (mFrameSub.has_value()) {
        static_cast<void>(mApi.unsubscribeFrame(*mFrameSub));
        mFrameSub.reset();
    }
    mLastClockSecond = -1;
}

void Ex04Calendar::onFrame() {
    if (!mContextId.has_value()) return;
    ++mFrameCount;
    if (mFrameCount % 30 != 0) return;

    std::tm t{};
    nowParts(t);
    std::int64_t sec = static_cast<std::int64_t>(t.tm_hour) * 3600 + static_cast<std::int64_t>(t.tm_min) * 60 + t.tm_sec;
    if (sec == mLastClockSecond) return;
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