#include "mod/examples/ex07_calendar.h"

#include "mod/MyMod.h"

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
// Page script. Pushed as a <script> DomNode at the end of the ComponentSpec
// body (the verified injection channel; DOM <script> nodes / eval() crash the
// engine).
// R1/R2/R3: the whole layout skeleton is DECLARED in the C++ component tree
// (see registerAll) - full-screen flex root, header, 6x7 grid and bottom are
// injected by the renderer as flex-only DOM. This script never builds UI; it
// only: 1) caches refs to the component-tree nodes, 2) sizes the flexible
// regions once (geometry: flex + explicit dimension anchors, resize-only,
// no per-pixel coordinates), 3) fills data with a DOM-diff render (only
// changed cells are written), 4) delegates clicks (component synthesised
// clicks are unreliable). Every visual token (deep background, thin borders,
// rounded corners, green accent) mirrors the vanilla dark component renderer.
// ---------------------------------------------------------------------------
constexpr const char* kPageScript = R"js((function () {
  // R1/R2/R3: the layout skeleton is declared entirely by the C++ component
  // tree (see registerAll). This script NEVER builds UI; it only:
  //   1) caches refs to the component-tree nodes (cache),
  //   2) sizes flexible regions once (geometry: flex + explicit dimension
  //      anchors, resize-only - no per-pixel left/top coordinates),
  //   3) fills data with a DOM-diff render (only changed cells are written),
  //   4) delegates clicks (component synthesised clicks are unreliable).
  var F = "'Microsoft YaHei','SimHei','Noto Sans SC','Noto Sans','Segoe UI',sans-serif";
  var WHITE='#ffffff', LIGHT='#d0d7de', DARK='#161b22', PANEL='#21262d', BORDER='#30363d', GREEN='#3fb950';
  var state={y:0,m:0,today:'',selected:'',events:{}};
  var week=['日','一','二','三','四','五','六'];
  function pad(n){return n<10?'0'+n:''+n;}
  function key(y,m,d){return y+'-'+pad(m+1)+'-'+pad(d);}
  function el(tag,id){var n=document.createElement(tag);if(id)n.id=id;return n;}
  function css(n,s){n.style.cssText=s;}
  function setP(n,k,v){n.style.setProperty(k,v);} // single-property write: never clobbers sibling-owner props
  function text(n,s){n.textContent=s;}
  function has(k){return state.events[k]&&state.events[k].length>0;}
  var R={},last={cells:[],title:'',day:''};
  var tries=0;
  function boot(){
    tries++; var root=document.getElementById('cal-root');
    if(!root||!window.oreui){if(tries<60)setTimeout(boot,50);return;}
    var d=new Date(); state.y=d.getFullYear();state.m=d.getMonth();state.today=key(state.y,state.m,d.getDate());state.selected=state.today;
    // Single-property writes: never clobber the component-tree layout props
    // (position:fixed/display:flex on #cal-root) with a full cssText overwrite.
    setP(root,'font-family',F);setP(root,'color',WHITE);setP(root,'user-select','none');
    cache(); geometry(); render(); bind(root);
    window.addEventListener('resize',geometry);
    window.oreui.event.on('calendar.events',function(p){if(p&&p.events)state.events=p.events;render();});
    window.oreui.event.on('calendar.clock',function(p){var c=R.clock;if(c)text(c,pad(p.h||0)+':'+pad(p.mi||0)+':'+pad(p.s||0));var k=key(p.y||0,(p.m||1)-1,p.d||1);if(k!==state.today){var old=state.today;state.today=k;if(state.selected===old){state.selected=k;state.y=p.y||state.y;state.m=(p.m||1)-1;}render();}});
  }
  // R2: cache refs to the component-tree skeleton; build the event-list row
  // pool once (3 rows + empty state). No layout skeleton is fabricated here.
  function cache(){
    R.root=document.getElementById('cal-root');
    R.grid=document.getElementById('cal-grid');R.bottom=document.getElementById('cal-bottom');
    R.title=document.getElementById('cal-title');R.prev=document.getElementById('cal-prev');R.todayB=document.getElementById('cal-today');R.next=document.getElementById('cal-next');
    R.day=document.getElementById('cal-day');R.list=document.getElementById('cal-list');R.input=document.getElementById('cal-input');R.add=document.getElementById('cal-add');R.clock=document.getElementById('cal-clock');
    R.cells=R.grid?R.grid.querySelectorAll('[data-index]'):[];
    R.rows=[];R.empty=null;
    if(R.list){
      R.empty=el('div');text(R.empty,'这一天没有事件');css(R.empty,'height:36px;line-height:36px;text-align:center;color:'+LIGHT+';font-size:15px;');R.list.appendChild(R.empty);
      for(var i=0;i<3;i++){var row=el('div');row.setAttribute('data-del',String(i));css(row,'position:relative;height:30px;line-height:30px;text-align:center;color:'+WHITE+';font-size:15px;border-bottom:1px solid '+BORDER+';cursor:pointer;');var label=el('span');label.className='cal-ev';text(label,'');css(label,'display:flex;align-items:center;justify-content:center;width:100%;height:30px;');row.appendChild(label);var del=el('span');text(del,'×');css(del,'position:absolute;right:8px;top:0;width:24px;height:30px;line-height:30px;text-align:center;color:'+LIGHT+';');row.appendChild(del);R.rows.push(row);R.list.appendChild(row);row.style.display='none';}
    }
  }
  // R1: resize-only sizing with flex + explicit dimension anchors. Flex
  // distributes header/grid/bottom and the 6x7 grid cells; no per-pixel
  // left/top coordinates are ever written (render() writes none at all).
  function geometry(){
    var root=R.root;if(!root||!R.grid)return;
    var W=root.clientWidth||window.innerWidth||1280,H=root.clientHeight||window.innerHeight||720;
    var mx=Math.floor(W*0.10), my=Math.floor(H*0.10), CW=Math.max(320,W-2*mx), CH=Math.max(420,H-2*my);
    var bottomH=Math.min(245,Math.floor(CH*0.30));
    // Keep the whole calendar inside a centered 80% viewport rectangle.
    setP(root,'padding',my+'px '+mx+'px');
    setP(R.grid,'flex','1 1 auto');
    if(R.bottom){setP(R.bottom,'height',bottomH+'px');setP(R.bottom,'flex','none');}
    var rows=R.grid.querySelectorAll('[data-gridrow]');
    for(var i=0;i<rows.length;i++)setP(rows[i],'flex','1 1 auto');
  }
  // R3: DOM-diff render - a memory snapshot gates every write, so a state
  // change touches only the cells that actually changed. No geometry writes.
  function render(){
    var grid=R.grid;if(!grid)return;
    var first=new Date(state.y,state.m,1),lead=first.getDay(),days=new Date(state.y,state.m+1,0).getDate(),prev=new Date(state.y,state.m,0).getDate();
    var cells=R.cells;
    for(var j=0;j<cells.length;j++){
      var m=state.m,y=state.y,dn,cls='cur';if(j<lead){cls='prev';m--;dn=prev-lead+1+j;}else if(j>=lead+days){cls='next';m++;dn=j-lead-days+1;}else dn=j-lead+1;
      var k=key(y,m,dn),sel=k===state.selected,today=k===state.today;
      var c=cells[j],L=last.cells[j]||(last.cells[j]={});
      if(L.date!==k){c.setAttribute('data-date',k);L.date=k;}
      if(L.dn!==dn){text(c,String(dn));L.dn=dn;}
      var bg=sel?'rgba(63,185,80,.85)':today?'rgba(63,185,80,.18)':'transparent';
      if(L.bg!==bg){setP(c,'background',bg);L.bg=bg;}
      var bc=today?GREEN:'transparent';
      if(L.bc!==bc){setP(c,'border-color',bc);L.bc=bc;}
      var col=cls==='cur'?WHITE:'rgba(255,255,255,.35)';
      if(L.col!==col){setP(c,'color',col);L.col=col;}
      var h=has(k);
      if(L.dot!==h){
        var old=c.querySelectorAll('.cal-dot');for(var q=0;q<old.length;q++)old[q].parentNode.removeChild(old[q]);
        if(h){var dot=el('span');dot.className='cal-dot';text(dot,'•');css(dot,'position:absolute;right:7px;bottom:0;color:'+GREEN+';font-size:16px;line-height:14px;');c.appendChild(dot);}
        L.dot=h;
      }
    }
    if(R.title){var t=state.y+' 年 '+(state.m+1)+' 月';if(last.title!==t){text(R.title,t);last.title=t;}}
    var dt=state.selected.split('-'),dow=new Date(+dt[0],+dt[1]-1,+dt[2]).getDay(),evs=state.events[state.selected]||[];
    if(R.day){var d2=(+dt[1])+' 月 '+(+dt[2])+' 日 · 周'+week[dow]+(state.selected===state.today?'（今天）':'');if(last.day!==d2){text(R.day,d2);last.day=d2;}}
    // Event list via row pool (R3): reuse nodes, only fill text / toggle hide.
    if(R.list){
      var showEmpty=!evs.length;
      if(R.empty)R.empty.style.display=showEmpty?'':'none';
      for(var e=0;e<R.rows.length;e++){
        var row=R.rows[e],ev=evs[e];
        var vis=!showEmpty&&ev!==undefined;
        row.style.display=vis?'':'none';
        if(vis){var lb=row.querySelector('.cal-ev');if(lb)text(lb,'•  '+ev);}
      }
    }
  }
  function bind(root){root.addEventListener('click',function(e){var t=e.target,id=t&&t.id;if(id==='cal-prev'){state.m--;if(state.m<0){state.m=11;state.y--;}render();return;}if(id==='cal-next'){state.m++;if(state.m>11){state.m=0;state.y++;}render();return;}if(id==='cal-today'){var d=new Date();state.y=d.getFullYear();state.m=d.getMonth();state.selected=state.today;render();return;}if(id==='cal-add'){add();return;}while(t&&t!==root){if(t.getAttribute){var date=t.getAttribute('data-date'),del=t.getAttribute('data-del');if(date){state.selected=date;render();return;}if(del!==null){remove(+del);return;}}t=t.parentNode;}});var input=R.input;if(input)input.addEventListener('keydown',function(e){if(e.key==='Enter')add();});}
  function add(){var input=R.input,s=input&&input.value.trim();if(!s)return;if(!state.events[state.selected])state.events[state.selected]=[];state.events[state.selected].push(s);input.value='';render();}
  function remove(i){var a=state.events[state.selected];if(!a)return;a.splice(i,1);if(!a.length)delete state.events[state.selected];render();}
  boot();
})();
)js";

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
    body.push_back(dearoreui::api::DomNode{.tag = "script", .text = kPageScript});

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