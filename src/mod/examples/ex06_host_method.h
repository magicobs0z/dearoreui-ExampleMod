#pragma once

#include "api/IDearOreUIApi.h"
#include "api/IHostMethod.h"
#include "api/types/ComponentSpec.h"
#include "api/types/DomNode.h"

#include "mod/examples/ExampleBase.h"

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
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = kPageScript});

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

    static constexpr char kPageScript[] = R"js((function () {
  // Absolute-positioned month grid kernel + one JS->C++ dispatch that pulls
  // seed events (green dots). Heartbeat proves the page JS survives the
  // facet call (the binding channel would have killed it - 0x40080201).
  var PAD = 6, CELL_W = 38, CELL_H = 34, TITLE_H = 34, HEAD_H = 22, FIX_H = 40;
var FONT = 'Microsoft YaHei','SimHei','Noto Sans SC','Noto Sans','Segoe UI',sans-serif;
  var state = {
 viewY: 0, viewM: 0, selected: null, today: '', events: {} };
  var WEEK = ['日', '一', '二', '三', '四', '五', '六'];
  function pad(n) { return n < 10 ? '0' + n : '' + n; }
  function ymdStr(y, m, d) { return y + '-' + pad(m + 1) + '-' + pad(d); }
  function $(id) { return document.getElementById(id); }
  function cellStyleText(cls, isToday, isSel) {
    var base =
      'position:absolute;font-family:' + FONT + ';font-size:13px;' +
      'text-align:center;line-height:' + CELL_H + 'px;border-radius:6px;cursor:pointer;box-sizing:border-box;';
    var color = cls === 'cur' ? '#ffffff' : 'rgba(208,215,222,0.35)';
    var bg = 'transparent', ring = '';
    if (isSel) { bg = '#3fb950'; color = '#ffffff'; }
    else if (isToday) { bg = 'rgba(63,185,80,0.10)'; ring = 'box-shadow:inset 0 0 0 1px #3fb950;'; color = '#3fb950'; }
    return base + 'color:' + color + ';background:' + bg + ';' + ring + 'width:' + CELL_W + 'px;height:' + CELL_H + 'px;';
  }
  function hasEvents(full) {
    return state.events && Object.prototype.hasOwnProperty.call(state.events, full) && state.events[full].length > 0;
  }
  var attempts = 0, jsTick = 0;
  function boot() {
    attempts++;
    var root = $('cal-root');
    if (!root || !window.oreui) { if (attempts < 40) { setTimeout(boot, 50); } return; }
    var d = new Date();
    state.today = ymdStr(d.getFullYear(), d.getMonth(), d.getDate());
    state.viewY = d.getFullYear(); state.viewM = d.getMonth();
    buildStatic(root);
    renderCalendar();
    bindEvents(root);
    // Heartbeat: proves JS survives the facet dispatch below.
    try { setInterval(function () { jsTick++; var j = $('cal-heart'); if (j) j.textContent = 'js: ' + jsTick; }, 1000); } catch (e) {}
    // Single allowed JS->C++ dispatch -> seed events.
    try {
      window.oreui.host.call('calendar.init', { want: ['events'] }).then(function (res) {
        try {
          var parsed = JSON.parse(res);
          state.events = parsed && parsed.events ? parsed.events : {};
        } catch (e) { state.events = {}; }
        var n = 0;
        for (var k in state.events) { if (Object.prototype.hasOwnProperty.call(state.events, k)) { n += state.events[k].length; } }
        var info = $('cal-info');
        if (info) info.textContent = 'init: loaded ' + n + ' event(s) via facet';
        renderCalendar();
      }).catch(function (err) {
        var info = $('cal-info');
        if (info) info.textContent = 'init error: ' + String(err && err.message ? err.message : err);
      });
    } catch (e) {
      var info = $('cal-info');
      if (info) info.textContent = 'init call failed: ' + String(e);
    }
  }
  function gridTop() { return TITLE_H + HEAD_H; }
  function buildStatic(root) {
    root.style.height = (gridTop() + 6 * CELL_H + FIX_H) + 'px';
    root.innerHTML =
      '<div id="cal-title" style="position:absolute;left:' + PAD + 'px;top:6px;font-family:' + FONT + ';font-size:15px;font-weight:700;color:#ffffff;"></div>' +
      '<div id="cal-prev" style="position:absolute;right:' + (PAD + 58) + 'px;top:4px;width:24px;height:26px;font-family:' + FONT + ';font-size:14px;text-align:center;line-height:24px;color:#d0d7de;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">&#8249;</div>' +
      '<div id="cal-today" style="position:absolute;right:' + (PAD + 30) + 'px;top:4px;width:54px;height:26px;font-family:' + FONT + ';font-size:11px;text-align:center;line-height:24px;color:#3fb950;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">今天</div>' +
      '<div id="cal-next" style="position:absolute;right:' + PAD + 'px;top:4px;width:24px;height:26px;font-family:' + FONT + ';font-size:14px;text-align:center;line-height:24px;color:#d0d7de;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">&#8250;</div>' +
      '<div id="cal-info" style="position:absolute;left:' + PAD + 'px;top:' + (gridTop() + 6 * CELL_H + 4) + 'px;font-family:' + FONT + ';font-size:12px;color:#d0d7de;">dispatching init...</div>' +
      '<div id="cal-heart" style="position:absolute;right:' + PAD + 'px;top:' + (gridTop() + 6 * CELL_H + 20) + 'px;font-family:' + FONT + ';font-size:11px;color:rgba(208,215,222,0.35);">js: 0</div>';
    for (var i = 0; i < 7; i++) {
      var h = document.createElement('div');
      h.textContent = WEEK[i];
      h.style.cssText =
        'position:absolute;left:' + (PAD + i * CELL_W) + 'px;top:' + TITLE_H + 'px;' +
        'width:' + CELL_W + 'px;height:' + HEAD_H + 'px;font-family:' + FONT + ';' +
        'font-size:11px;text-align:center;line-height:' + HEAD_H + 'px;color:' + (i === 0 ? '#d0d7de' : '#d0d7de') + ';';
      root.appendChild(h);
    }
  }
  function renderCalendar() {
    var title = $('cal-title');
    if (title) { title.textContent = state.viewY + ' 年 ' + (state.viewM + 1) + ' 月'; }
    var first = new Date(state.viewY, state.viewM, 1);
    var lead = first.getDay();
    var daysInMonth = new Date(state.viewY, state.viewM + 1, 0).getDate();
    var prevDays = new Date(state.viewY, state.viewM, 0).getDate();
    var old = document.getElementById('cal-cells');
    if (old) { old.parentNode.removeChild(old); }
    var cells = document.createElement('div');
    cells.id = 'cal-cells';
    cells.style.cssText = 'position:absolute;left:' + PAD + 'px;top:' + gridTop() + 'px;';
    for (var i = 0; i < 42; i++) {
      var cell = document.createElement('div');
      var y = state.viewY, m = state.viewM, d;
      var cls = 'cur';
      if (i < lead) { cls = 'prev'; m = m - 1; d = prevDays - lead + 1 + i; }
      else if (i >= lead + daysInMonth) { cls = 'next'; m = m + 1; d = i - lead - daysInMonth + 1; }
      else { d = i - lead + 1; }
      var full = ymdStr(y, m, d);
      cell.textContent = d;
      cell.setAttribute('data-full', full);
      var px = (i % 7) * CELL_W, py = Math.floor(i / 7) * CELL_H;
      cell.style.cssText = cellStyleText(cls, full === state.today, full === state.selected) + 'left:' + px + 'px;top:' + py + 'px;';
      if (hasEvents(full)) {
        var dot = document.createElement('div');
        dot.style.cssText = 'position:absolute;bottom:3px;left:50%;width:4px;height:4px;margin-left:-2px;border-radius:50%;background:#3fb950;';
        cell.appendChild(dot);
      }
      cells.appendChild(cell);
    }
    var root = $('cal-root');
    if (root) root.appendChild(cells);
  }
  function dayCellFrom(target) {
    while (target && target !== document) {
      if (target.getAttribute && target.getAttribute('data-full')) return target;
      target = target.parentNode;
    }
    return null;
  }
  function bindEvents(root) {
    root.addEventListener('click', function (e) {
      var t = e.target || e.srcElement;
      if (t && t.id === 'cal-prev') { state.viewM--; if (state.viewM < 0) { state.viewM = 11; state.viewY--; } renderCalendar(); return; }
      if (t && t.id === 'cal-next') { state.viewM++; if (state.viewM > 11) { state.viewM = 0; state.viewY++; } renderCalendar(); return; }
      if (t && t.id === 'cal-today') {
        var d = new Date();
        state.viewY = d.getFullYear(); state.viewM = d.getMonth();
        state.selected = ymdStr(d.getFullYear(), d.getMonth(), d.getDate());
        renderCalendar(); return;
      }
      var cell = dayCellFrom(t);
      if (cell) {
        state.selected = cell.getAttribute('data-full');
        var n = hasEvents(state.selected) ? state.events[state.selected].length : 0;
        var info = $('cal-info');
        if (info) info.textContent = state.selected + ' (' + n + ' event' + (n === 1 ? '' : 's') + ')';
        renderCalendar();
      }
    });
  }
  boot();
})();
)js";

    dearoreui::api::ModId                             mModId;
    std::optional<dearoreui::api::RegistrationHandle> mHostHandle;
    std::optional<dearoreui::api::RegistrationHandle> mUiHandle;
};

} // namespace examples
} // namespace my_mod