#pragma once

#include "api/IDearOreUIApi.h"
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
        body.push_back(dearoreui::api::DomNode{.tag = "script", .text = kPageScript});

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

    static constexpr char kPageScript[] = R"js((function () {
  // Absolute-positioned month grid kernel + frame-driven clock bar.
  var PAD = 6, CELL_W = 38, CELL_H = 34, TITLE_H = 34, HEAD_H = 22, FIX_H = 26;
var FONT = 'Microsoft YaHei','SimHei','Noto Sans SC','Noto Sans','Segoe UI',sans-serif;
  var state = {
 viewY: 0, viewM: 0, selected: null, today: '' };
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
  var attempts = 0;
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
    // Lesson 05: frame-driven clock; cross-midnight today refresh.
    window.oreui.event.on('calendar.clock', function (payload) {
      var p = payload || {};
      var clock = $('cal-clock');
      if (clock) { clock.textContent = pad(p.h || 0) + ':' + pad(p.mi || 0) + ':' + pad(p.s || 0); }
      var ymd = ymdStr(p.y || 0, (p.m || 1) - 1, p.d || 1);
      if (ymd !== state.today) { state.today = ymd; renderCalendar(); }
    });
  }
  function gridTop() { return TITLE_H + HEAD_H; }
  function buildStatic(root) {
    root.style.height = (gridTop() + 6 * CELL_H + FIX_H) + 'px';
    root.innerHTML =
      '<div id="cal-title" style="position:absolute;left:' + PAD + 'px;top:6px;font-family:' + FONT + ';font-size:15px;font-weight:700;color:#ffffff;"></div>' +
      '<div id="cal-prev" style="position:absolute;right:' + (PAD + 58) + 'px;top:4px;width:24px;height:26px;font-family:' + FONT + ';font-size:14px;text-align:center;line-height:24px;color:#d0d7de;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">&#8249;</div>' +
      '<div id="cal-today" style="position:absolute;right:' + (PAD + 30) + 'px;top:4px;width:54px;height:26px;font-family:' + FONT + ';font-size:11px;text-align:center;line-height:24px;color:#3fb950;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">今天</div>' +
      '<div id="cal-next" style="position:absolute;right:' + PAD + 'px;top:4px;width:24px;height:26px;font-family:' + FONT + ';font-size:14px;text-align:center;line-height:24px;color:#d0d7de;background:#21262d;border:1px solid #30363d;border-radius:6px;cursor:pointer;">&#8250;</div>' +
      '<div id="cal-clock" style="position:absolute;left:' + PAD + 'px;top:' + (gridTop() + 6 * CELL_H + 4) + 'px;font-family:' + FONT + ';font-size:15px;font-weight:600;color:#7ee787;font-variant-numeric:tabular-nums;">--:--:--</div>';
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
        renderCalendar();
      }
    });
  }
  boot();
})();
)js";

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