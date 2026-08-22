#include "mod/StateCenter.h"

#include "mod/MyMod.h"

#include "api/IHostMethod.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/UiManifest.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"
#include "api/types/Page.h"

#include "ll/api/event/EventBus.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace my_mod {

namespace {

// ---------------------------------------------------------------------------
// Page script (registered through registerScript + UiManifest.scripts). Single
// JS->C++ dispatch: example.state.init. Everything else flows C++->JS events.
// Dynamic regions are the ComponentSpec-body containers (sc-tab-*).
// ---------------------------------------------------------------------------
constexpr char kStateCenterJs[] = R"js((function () {
  'use strict';
  function $(id) { return document.getElementById(id); }
  function esc(s) { return String(s == null ? '' : s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  // ---- Tab switching: renderer emits [data-component="tab"] in declaration
  // order (status / log / diag); body panels are the sc-tab-* containers.
  var tabs    = Array.prototype.slice.call(document.querySelectorAll('[data-component="tab"]'));
  var panels  = ['sc-tab-status', 'sc-tab-log', 'sc-tab-diag'];
  function activate(i) {
    for (var k = 0; k < panels.length; k++) {
      var p = $(panels[k]);
      if (p) p.style.display = (k === i) ? 'block' : 'none';
    }
    for (var t = 0; t < tabs.length; t++) {
      if (tabs[t]) tabs[t].style.opacity = (t === i) ? '1' : '0.55';
    }
  }
  for (var i = 0; i < tabs.length; i++) {
    (function (idx) {
      if (tabs[idx]) tabs[idx].addEventListener('click', function () { activate(idx); });
    })(i);
  }

  // ---- Status tab: hud.tick payload -> key/value rows.
  function renderStatus(payload) {
    var rows = $('sc-status-rows');
    if (!rows) return;
    var d = payload || {};
    rows.innerHTML =
      '<div>内存 <b>' + esc(d.memMb) + ' MB</b></div>' +
      '<div>运行 <b>' + esc(d.uptimeSec) + ' s</b></div>' +
      '<div>世界 tick <b>' + esc(d.tick) + '</b></div>' +
      '<div>协议 <b>' + esc(d.protocol) + '</b> / ready=' + (d.ready ? '<b style="color:#7ee787;">true</b>' : '<b style="color:#ff7b72;">false</b>') + '</div>' +
      '<div>本 Mod UI 句柄 <b>' + esc(d.uiHandles) + '</b></div>' +
      '<div>DearOreUI <b>' + esc(d.runtimeVersion) + '</b></div>';
  }

  // ---- Log tab: log.append payload -> appended lines.
  function renderLog(payload) {
    var box = $('sc-log');
    if (!box) return;
    var d = payload || {};
    var lines = d.lines || [];
    for (var k = 0; k < lines.length; k++) {
      var line = lines[k];
      var color = '#d0d1d4';
      if (line.level === 'warn') color = '#ffa657';
      if (line.level === 'error') color = '#ff7b72';
      var el = document.createElement('div');
      el.style.color = color;
      el.textContent = '[' + line.ts + '] [' + line.level + '] ' + line.message;
      box.appendChild(el);
    }
    if (lines.length && box.scrollTop != null) box.scrollTop = box.scrollHeight;
  }

  // ---- Diagnostics tab: diag.refresh payload -> full list rerender.
  function renderDiag(payload) {
    var box = $('sc-diag');
    if (!box) return;
    var d = payload || {};
    var items = d.items || [];
    box.innerHTML = '';
    if (!items.length) {
      box.textContent = '(no diagnostics)';
      return;
    }
    for (var k = 0; k < items.length; k++) {
      var it = items[k];
      var color = '#d0d1d4';
      if (it.severity === 'warning') color = '#ffa657';
      if (it.severity === 'error' || it.severity === 'critical') color = '#ff7b72';
      var el = document.createElement('div');
      el.style.color = color;
      el.style.fontSize = '11px';
      el.textContent = '[' + it.ts + '] ' + it.category + '/' + it.event +
        (it.mod ? ' @' + it.mod : '') + ': ' + it.message;
      box.appendChild(el);
    }
    box.scrollTop = box.scrollHeight;
  }

  window.oreui.event.on('hud.tick', renderStatus);
  window.oreui.event.on('log.append', renderLog);
  window.oreui.event.on('diag.refresh', renderDiag);

  // Single allowed JS->C++ dispatch (ViewDispatchAlreadyUsed after this).
  try {
    window.oreui.host.call('example.state.init', { want: ['status', 'logs', 'diag'] }).then(function (res) {
      try {
        var all = JSON.parse(res);
        if (all.status) renderStatus(all.status);
        if (all.logs) renderLog(all.logs);
        if (all.diag) renderDiag(all.diag);
      } catch (e) {}
    }).catch(function () {
      try { renderStatus({ memMb: '?', uptimeSec: '?', tick: '?', protocol: '?', ready: false, uiHandles: 0, runtimeVersion: '?' }); } catch (e) {}
    });
  } catch (e) {}

  activate(0);
})();
)js";

// example.state.init owner: batch initial snapshot.
class StateInitMethod final : public dearoreui::api::IHostMethod {
public:
    explicit StateInitMethod(StateCenter& center) : mCenter(center) {}
    [[nodiscard]] std::string name() const override { return "example.state.init"; }
    [[nodiscard]] dearoreui::api::Permission requiredPermission() const override {
        return dearoreui::api::Permission::HostReadOnly;
    }
    [[nodiscard]] dearoreui::api::Result<std::string>
    execute(dearoreui::api::ContextId contextId, std::string_view args) override {
        return dearoreui::api::Result<std::string>::success(mCenter.handleInit(args));
    }

private:
    StateCenter& mCenter;
};

} // namespace

StateCenter::StateCenter(dearoreui::api::IDearOreUIApi& api, dearoreui::api::ModId modId, MyMod& mod)
: mApi(api),
  mModId(std::move(modId)),
  mMod(mod) {
    mStarted = std::chrono::steady_clock::now();
    appendLog("info", "state center created");
}

StateCenter::~StateCenter() { shutdown(); }

std::string StateCenter::jsonEscape(std::string const& value) const {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    return out;
}

std::string StateCenter::collectMemoryMb() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        auto mb = counters.WorkingSetSize / (1024ULL * 1024ULL);
        return std::to_string(mb);
    }
#endif
    return "0";
}

std::string StateCenter::collectStatusJson() const {
    auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - mStarted).count();
    std::string json;
    json  = "{";
    json += "\"memMb\":" + collectMemoryMb() + ",";
    json += "\"uptimeSec\":" + std::to_string(uptimeSec) + ",";
    json += "\"tick\":" + std::to_string(mTick) + ",";
    json += "\"protocol\":" + std::to_string(mApi.getProtocolVersion()) + ",";
    json += "\"ready\":" + std::string(mApi.isReady() ? "true" : "false") + ",";
    json += "\"uiHandles\":" + std::to_string(mUiHandle.has_value() ? 1 : 0) + ",";
    std::string runtimeVersion = mApi.getInfo().modVersion.toString();
    if (runtimeVersion.empty()) runtimeVersion = "?";
    json += "\"runtimeVersion\":\"" + jsonEscape(runtimeVersion) + "\"";
    json += "}";
    return json;
}

std::string StateCenter::collectLogsJson(std::size_t from) const {
    std::string json = "{\"lines\":[";
    bool        first = true;
    for (auto const& line : mLogs) {
        if (first) {
            first = false;
        } else {
            json += ",";
        }
        json += "{\"ts\":\"" + std::to_string(line.timestampMs) + "\",\"level\":\"" + line.level
            + "\",\"message\":\"" + jsonEscape(line.message) + "\"}";
    }
    json += "]}";
    return json;
}

std::string StateCenter::collectDiagJson(std::size_t limit) const {
    dearoreui::api::DiagnosticQuery query;
    query.requester      = mModId;
    query.minimumSeverity = dearoreui::api::DiagnosticSeverity::Debug;
    query.limit          = limit;
    auto result          = mApi.queryDiagnostics(query);
    std::string json     = "{\"items\":[";
    if (result.isOk()) {
        bool first = true;
        for (auto const& item : result.value().items) {
            if (first) {
                first = false;
            } else {
                json += ",";
            }
            constexpr const char* kSeverityNames[] = {"debug", "info", "warning", "error", "critical"};
            auto severity = static_cast<std::size_t>(item.severity);
            if (severity >= 5) severity = 1;
            json += "{\"ts\":\"" + std::to_string(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            item.timestamp.time_since_epoch()
                        ).count()
                    ) + "\",\"severity\":\"" + kSeverityNames[severity] + "\",\"category\":\""
                + jsonEscape(item.category) + "\",\"event\":\"" + jsonEscape(item.event)
                + "\",\"message\":\"" + jsonEscape(item.message) + "\"";
            if (item.mod && item.mod->isValid()) {
                json += ",\"mod\":\"" + jsonEscape(item.mod->value()) + "\"";
            }
            json += "}";
        }
    }
    json += "]}";
    return json;
}

void StateCenter::appendLog(std::string level, std::string message) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    if (mLogs.size() >= 256) {
        mLogs.pop_front();
    }
    mLogs.push_back(LogLine{static_cast<std::uint64_t>(now), std::move(level), std::move(message)});
}

std::string StateCenter::handleInit(std::string_view /*args*/) {
    appendLog("info", "page dispatched example.state.init");
    std::string json = "{";
    json += "\"status\":" + collectStatusJson() + ",";
    json += "\"logs\":" + collectLogsJson() + ",";
    json += "\"diag\":" + collectDiagJson();
    json += "}";
    return json;
}

void StateCenter::publishStatus() {
    if (!mContextId.has_value()) return;
    dearoreui::api::EventPublishOptions options;
    options.owner   = mModId;
    options.context = *mContextId;
    options.name    = "hud.tick";
    options.payload = collectStatusJson();
    auto result     = mApi.publishEvent(options);
    if (result.isErr()) {
        appendLog("error", "hud.tick publish failed: " + result.error().message);
    }
}

void StateCenter::publishLogs() {
    if (!mContextId.has_value()) return;
    dearoreui::api::EventPublishOptions options;
    options.owner   = mModId;
    options.context = *mContextId;
    options.name    = "log.append";
    options.payload = collectLogsJson();
    auto result     = mApi.publishEvent(options);
    if (result.isErr()) {
        appendLog("error", "log.append publish failed: " + result.error().message);
    }
    mLastLogIndex = mLogs.size();
}

void StateCenter::publishDiag() {
    if (!mContextId.has_value()) return;
    dearoreui::api::EventPublishOptions options;
    options.owner   = mModId;
    options.context = *mContextId;
    options.name    = "diag.refresh";
    options.payload = collectDiagJson(200);
    auto result     = mApi.publishEvent(options);
    if (result.isErr()) {
        appendLog("error", "diag.refresh publish failed: " + result.error().message);
    }
}

void StateCenter::onClientTick(ll::event::ClientLevelTickEvent& /*event*/) {
    ++mTick;
    if (mTick % 20 == 0) {
        publishStatus();
        if (mTick % 100 == 0) {
            publishDiag();
        }
        if (mLogs.size() > mLastLogIndex) {
            publishLogs();
        }
    }
}

void StateCenter::onPageReady(dearoreui::api::PageContextView const& view) {
    mContextId = view.id;
    appendLog("info", "page ready context " + std::to_string(view.id.value()));
    publishStatus();
    publishLogs();
    publishDiag();
}

void StateCenter::onPageDestroyed(dearoreui::api::PageContextView const& view) {
    appendLog("info", "page destroyed context " + std::to_string(view.id.value()));
    mContextId.reset();
}

bool StateCenter::registerAll() {
    auto& logger = mMod.getSelf().getLogger();

    // 1. Script resource (page interactions; UiManifest.scripts references it).
    dearoreui::api::ScriptManifest scriptManifest;
    scriptManifest.modNamespace = mModId.value();
    scriptManifest.path         = "state_center.js";
    scriptManifest.fingerprint  = "state_center.1";
    scriptManifest.pageScopes   = {dearoreui::api::PageScope::Any};
    auto script = mApi.registerScript(mModId, scriptManifest, std::string(kStateCenterJs));
    if (script.isErr()) {
        logger.error("registerScript failed: {}", script.error().message);
        return false;
    }
    mScriptHandle = script.value();

    // 2. Declarative component UI (vanilla-rendered panel + tabs + body
    // containers that the page script fills).
    dearoreui::api::UiManifest uiManifest;
    uiManifest.modNamespace = mModId.value();
    uiManifest.id           = "state_center";
    uiManifest.kind         = dearoreui::api::UiKind::Overlay;
    uiManifest.pageScopes   = {dearoreui::api::PageScope::Any};
    uiManifest.anchor       = dearoreui::api::UiAnchor::TopRight;
    uiManifest.pointerEvents = true; // tab clicks need pointer input
    uiManifest.containerId =
        dearoreui::api::makeUiContainerId(uiManifest.modNamespace, uiManifest.kind, uiManifest.id);
    uiManifest.fingerprint = "state_center.v1";
    uiManifest.scripts     = {"state_center.js"};

    dearoreui::api::ComponentSpec panel;
    panel.kind  = dearoreui::api::ComponentKind::Panel;
    panel.style = "dark";
    panel.label = "状态中心";

    dearoreui::api::ComponentSpec tabBar;
    tabBar.kind = dearoreui::api::ComponentKind::TabBar;
    auto makeTab = [](std::string label) {
        dearoreui::api::ComponentSpec tab;
        tab.kind  = dearoreui::api::ComponentKind::Button;
        tab.label = std::move(label);
        return tab;
    };
    tabBar.children = {makeTab("状态"), makeTab("日志"), makeTab("诊断")};
    panel.children.push_back(std::move(tabBar));

    auto makePanelDiv = [](std::string id, std::string mainId, bool hidden) {
        dearoreui::api::DomNode node;
        node.tag   = "div";
        node.attrs.push_back(dearoreui::api::DomAttr{"id", std::move(id)});
        node.style = "font-family:Minecraft Seven v2;font-size:12px;line-height:1.8;"
                     "color:#d0d1d4;padding:0.6rem 0.2rem;";
        if (hidden) {
            node.style += "display:none;";
        }
        dearoreui::api::DomNode inner;
        inner.tag = "div";
        inner.attrs.push_back(dearoreui::api::DomAttr{"id", std::move(mainId)});
        inner.style = "max-height:420px;overflow-y:auto;word-break:break-all;";
        node.children.push_back(std::move(inner));
        return node;
    };
    panel.body = {
        makePanelDiv("sc-tab-status", "sc-status-rows", false),
        makePanelDiv("sc-tab-log", "sc-log", true),
        makePanelDiv("sc-tab-diag", "sc-diag", true),
    };

    auto ui = mApi.registerComponent(mModId, uiManifest, panel);
    if (ui.isErr()) {
        logger.error("registerComponent failed: {}", ui.error().message);
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
        return false;
    }
    mUiHandle = ui.value();
    appendLog("info", "component ui registered");

    // 3. Host method (single dispatch target).
    dearoreui::api::HostMethodManifest hostManifest;
    hostManifest.name        = "example.state.init";
    hostManifest.pageScopes  = {dearoreui::api::PageScope::Any};
    hostManifest.permissions = dearoreui::api::PermissionSet{
        std::vector{dearoreui::api::Permission::HostReadOnly}
    };
    auto host = mApi.registerHostMethod(
        mModId,
        hostManifest,
        std::make_shared<StateInitMethod>(*this)
    );
    if (host.isErr()) {
        logger.error("registerHostMethod failed: {}", host.error().message);
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
        return false;
    }
    mHostMethodHandle = host.value();

    // 4. Page lifecycle.
    dearoreui::api::PageSubscriptionOptions subOptions;
    subOptions.owner  = mModId;
    subOptions.scopes = {dearoreui::api::PageScope::Any};
    auto ready = mApi.subscribePage(
        subOptions,
        dearoreui::api::PageEvent::Ready,
        [this](dearoreui::api::PageContextView const& view) { onPageReady(view); }
    );
    if (ready.isErr()) {
        logger.error("subscribePage(Ready) failed: {}", ready.error().message);
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
        return false;
    }
    mReadySub = ready.value();

    auto destroyed = mApi.subscribePage(
        subOptions,
        dearoreui::api::PageEvent::Destroyed,
        [this](dearoreui::api::PageContextView const& view) { onPageDestroyed(view); }
    );
    if (destroyed.isErr()) {
        logger.error("subscribePage(Destroyed) failed: {}", destroyed.error().message);
        static_cast<void>(mApi.unsubscribePage(*mReadySub));
        mReadySub.reset();
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
        return false;
    }
    mDestroyedSub = destroyed.value();

    // 5. LL world tick listener (client main thread).
    auto& bus = ll::event::EventBus::getInstance();
    mTickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>(
        [this](ll::event::ClientLevelTickEvent& event) { onClientTick(event); }
    );
    if (mTickListener == nullptr) {
        logger.error("emplaceListener(ClientLevelTickEvent) failed");
        static_cast<void>(mApi.unsubscribePage(*mDestroyedSub));
        mDestroyedSub.reset();
        static_cast<void>(mApi.unsubscribePage(*mReadySub));
        mReadySub.reset();
        static_cast<void>(mApi.unregisterHostMethod(*mHostMethodHandle));
        mHostMethodHandle.reset();
        static_cast<void>(mApi.unregisterUi(*mUiHandle));
        mUiHandle.reset();
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
        return false;
    }
    appendLog("info", "fully registered");
    return true;
}

void StateCenter::shutdown() {
    auto& logger = mMod.getSelf().getLogger();

    if (mTickListener) {
        auto& bus = ll::event::EventBus::getInstance();
        bus.removeListener(mTickListener);
        mTickListener.reset();
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
    if (mScriptHandle.has_value()) {
        static_cast<void>(mApi.unregister(*mScriptHandle));
        mScriptHandle.reset();
    }
    mContextId.reset();
    mLogs.clear();
    mLogs.shrink_to_fit();
    appendLog("info", "shutdown complete");
    static_cast<void>(mApi.unregisterMod(mModId));
}

} // namespace my_mod