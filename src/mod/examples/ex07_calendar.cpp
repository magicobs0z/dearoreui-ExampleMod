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
// engine). Layout is 100% absolute positioning with explicit pixel geometry -
// the engine demonstrably honors it (CSS Grid/Flex were NOT reliable and
// scattered days across the screen in the first attempt). Every visual token
// (deep background, thin borders, rounded corners, blue accent) mirrors the
// vanilla dark component renderer.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Page script - COMPONENT-DRIVEN build.
// The page is declared as a DearOreUI component tree (see registerAll): the
// renderer creates every visible element (Button/Text/Input/ListItem/Grid/
// Stack) with its vanilla texture. This script never fabricates UI; it only:
//   1) turns the section root into a fixed full-screen black wash,
//   2) re-lays the component elements with explicit absolute coordinates
//      (CSS grid/flex proven unreliable on this engine page context),
//   3) fills data (dates, events, clock), rewrites CJK text on the component
//      nodes (component labels must stay ASCII - theme font has no CJK),
//   4) delegates clicks (component synthesised clicks are unreliable).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Page script - COMPONENT-DRIVEN build.
// The page is a DearOreUI component tree (see registerAll): the renderer
// creates every visible element (Button/Text/Input/ListItem/Grid/Stack) with
// its vanilla texture. This script never fabricates UI; it only:
//   1) turns the section root into a fixed full-screen black wash,
//   2) re-lays component elements with explicit absolute coordinates
//      (CSS grid/flex proven unreliable on this engine page context),
//   3) fills data (dates/events/clock) and rewrites CJK on component nodes
//      (component labels must be ASCII - theme font has no CJK glyphs),
//   4) delegates clicks (component synthesised clicks are unreliable).
// DOM contract (renderer emits in this order inside section):
//   grid(grid)          > 7 Text (weekday) + 42 Button (day cells)
//   nav(stack row)      > Button, Text(month), Button, Button
//   ev(stack column)    > Text(day title), slots(stack), row(stack), Text(clock)
//   slots.stack         > 4 x ListItem[ Text, Button(del) ]
//   row.stack           > Input + Button(add)
// ---------------------------------------------------------------------------
constexpr const char* kPageScript = R"js((function () {
  // R5: layout geometry is computed once by geometry() (initial build +
  // window resize). render() never writes top/left/width/height/right/bottom
  // (the engine pays O(4^Depth) for those) and only touches textContent and
  // paint-only props (background/border-color/color) plus idempotent dots.
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
  var tries=0;
  function boot(){
    tries++; var root=document.getElementById('cal-root');
    if(!root||!window.oreui){if(tries<60)setTimeout(boot,50);return;}
    var d=new Date(); state.y=d.getFullYear();state.m=d.getMonth();state.today=key(state.y,state.m,d.getDate());state.selected=state.today;
    css(root,'position:fixed;top:0;left:0;right:0;bottom:0;overflow:hidden;background:rgba(0,0,0,.95);font-family:'+F+';color:'+WHITE+';user-select:none;');
    build(root); render(); bind(root);
    window.addEventListener('resize',geometry);
    window.oreui.event.on('calendar.events',function(p){if(p&&p.events)state.events=p.events;render();});
    window.oreui.event.on('calendar.clock',function(p){var c=document.getElementById('cal-clock');if(c)text(c,pad(p.h||0)+':'+pad(p.mi||0)+':'+pad(p.s||0));var k=key(p.y||0,(p.m||1)-1,p.d||1);if(k!==state.today){var old=state.today;state.today=k;if(state.selected===old){state.selected=k;state.y=p.y||state.y;state.m=(p.m||1)-1;}render();}});
  }
  function build(root){
    root.innerHTML='';
    var header=el('div','cal-header');css(header,'position:absolute;background:'+PANEL+';border-bottom:2px solid '+BORDER+';');root.appendChild(header);
    var titleEl=el('div','cal-title');css(titleEl,'position:absolute;line-height:60px;text-align:center;font-size:26px;font-weight:bold;');header.appendChild(titleEl);
    addButton(header,'cal-prev','‹','position:absolute;');
    addButton(header,'cal-today','今天','position:absolute;');
    addButton(header,'cal-next','›','position:absolute;');
    var grid=el('div','cal-grid');css(grid,'position:absolute;');root.appendChild(grid);
    for(var i=0;i<7;i++){var h=el('div');h.setAttribute('data-week',String(i));text(h,week[i]);css(h,'position:absolute;top:0;height:34px;line-height:34px;text-align:center;font-size:16px;color:'+LIGHT+';');grid.appendChild(h);}
    for(var j=0;j<42;j++){var cell=el('div');cell.setAttribute('data-index',String(j));css(cell,'position:absolute;box-sizing:border-box;text-align:center;font-size:18px;cursor:pointer;border:1px solid transparent;border-radius:4px;color:'+WHITE+';');grid.appendChild(cell);}
    var bottom=el('div','cal-bottom');css(bottom,'position:absolute;background:rgba(22,27,34,.94);border-top:2px solid '+BORDER+';');root.appendChild(bottom);
    var day=el('div','cal-day');css(day,'position:absolute;line-height:32px;text-align:center;font-size:18px;font-weight:bold;border-bottom:1px solid '+BORDER+';');bottom.appendChild(day);
    var list=el('div','cal-list');css(list,'position:absolute;overflow:hidden;');bottom.appendChild(list);
    var input=el('input','cal-input');input.setAttribute('placeholder','输入事件…');css(input,'position:absolute;box-sizing:border-box;text-align:center;font-family:'+F+';font-size:15px;color:'+WHITE+';background:'+DARK+';border:1px solid '+BORDER+';border-radius:5px;');bottom.appendChild(input);
    addButton(bottom,'cal-add','添加','position:absolute;');
    var addB=document.getElementById('cal-add');if(addB){setP(addB,'background',GREEN);setP(addB,'color','#0d1117');}
    var clock=el('div','cal-clock');css(clock,'position:absolute;line-height:32px;text-align:center;color:'+GREEN+';font-size:22px;font-weight:bold;');text(clock,'--:--:--');bottom.appendChild(clock);
    geometry();
  }
  function addButton(parent,id,label,pos){var b=el('div',id);text(b,label);css(b,pos+'box-sizing:border-box;line-height:36px;text-align:center;font-family:'+F+';font-size:17px;color:'+LIGHT+';background:'+DARK+';border:0;outline:0;box-shadow:none;border-radius:5px;cursor:pointer;');parent.appendChild(b);}
  // R5: the ONLY place that writes layout geometry (build + resize). Uses
  // per-property setProperty so build() identity styles stay intact.
  function geometry(){
    var root=document.getElementById('cal-root'),grid=document.getElementById('cal-grid');if(!root||!grid)return;
    var W=root.clientWidth||window.innerWidth||1280,H=root.clientHeight||window.innerHeight||720;
    // Keep the complete calendar inside a centered 80% viewport rectangle.
    var mx=Math.floor(W*0.10), my=Math.floor(H*0.10), CW=Math.max(320,W-2*mx), CH=Math.max(420,H-2*my);
    var bottomH=Math.min(245,Math.floor(CH*0.30)), gridH=Math.max(260,CH-60-bottomH), cw=CW/7, ch=gridH/6;
    var header=document.getElementById('cal-header'), bottom=document.getElementById('cal-bottom');
    if(header){setP(header,'left',mx+'px');setP(header,'top',my+'px');setP(header,'width',CW+'px');setP(header,'height','60px');}
    if(bottom){setP(bottom,'left',mx+'px');setP(bottom,'top',(my+CH-bottomH)+'px');setP(bottom,'width',CW+'px');setP(bottom,'height',bottomH+'px');}
    var titleEl=document.getElementById('cal-title');
    if(titleEl){setP(titleEl,'left','0px');setP(titleEl,'top','0px');setP(titleEl,'width',CW+'px');setP(titleEl,'height','60px');}
    var prev=document.getElementById('cal-prev'),todayB=document.getElementById('cal-today'),next=document.getElementById('cal-next');
    if(prev){setP(prev,'right','190px');setP(prev,'top','12px');setP(prev,'width','48px');setP(prev,'height','36px');}
    if(todayB){setP(todayB,'right','120px');setP(todayB,'top','12px');setP(todayB,'width','60px');setP(todayB,'height','36px');}
    if(next){setP(next,'right','54px');setP(next,'top','12px');setP(next,'width','48px');setP(next,'height','36px');}
    var ix=20, iw=CW-40;
    var dayEl=document.getElementById('cal-day'), listEl=document.getElementById('cal-list'), inputEl=document.getElementById('cal-input'), addEl=document.getElementById('cal-add'), clockEl=document.getElementById('cal-clock');
    if(dayEl){setP(dayEl,'left',ix+'px');setP(dayEl,'top','12px');setP(dayEl,'width',iw+'px');setP(dayEl,'height','32px');}
    if(listEl){setP(listEl,'left',ix+'px');setP(listEl,'top','52px');setP(listEl,'width',iw+'px');setP(listEl,'height',Math.max(70,bottomH-140)+'px');}
    if(inputEl){setP(inputEl,'left',ix+'px');setP(inputEl,'bottom','55px');setP(inputEl,'width',(iw-112)+'px');setP(inputEl,'height','38px');}
    if(addEl){setP(addEl,'left',(ix+iw-100)+'px');setP(addEl,'top',(bottomH-93)+'px');setP(addEl,'width','100px');setP(addEl,'height','38px');setP(addEl,'line-height','38px');}
    if(clockEl){setP(clockEl,'left',ix+'px');setP(clockEl,'top',(bottomH-45)+'px');setP(clockEl,'width',iw+'px');setP(clockEl,'height','32px');}
    setP(grid,'left',mx+'px');setP(grid,'top',(my+60)+'px');setP(grid,'width',CW+'px');setP(grid,'height',gridH+'px');
    var hs=grid.querySelectorAll('[data-week]');for(var i=0;i<hs.length;i++){setP(hs[i],'left',(i*cw)+'px');setP(hs[i],'width',cw+'px');}
    var cells=grid.querySelectorAll('[data-index]');for(var j=0;j<cells.length;j++){setP(cells[j],'left',(j%7*cw+2)+'px');setP(cells[j],'top',(34+Math.floor(j/7)*ch+2)+'px');setP(cells[j],'width',(cw-4)+'px');setP(cells[j],'height',(ch-4)+'px');setP(cells[j],'line-height',(ch-4)+'px');}
  }
  // R5: dynamic updates only - textContent + paint-only props + idempotent
  // dots. No geometry writes here (geometry() owns left/top/width/height).
  function render(){
    var grid=document.getElementById('cal-grid');if(!grid)return;
    var first=new Date(state.y,state.m,1),lead=first.getDay(),days=new Date(state.y,state.m+1,0).getDate(),prev=new Date(state.y,state.m,0).getDate();
    var cells=grid.querySelectorAll('[data-index]');for(var j=0;j<cells.length;j++){
      var m=state.m,y=state.y,dn,cls='cur';if(j<lead){cls='prev';m--;dn=prev-lead+1+j;}else if(j>=lead+days){cls='next';m++;dn=j-lead-days+1;}else dn=j-lead+1;
      var k=key(y,m,dn),sel=k===state.selected,today=k===state.today;
      cells[j].setAttribute('data-date',k);text(cells[j],String(dn));
      setP(cells[j],'background',sel?'rgba(63,185,80,.85)':today?'rgba(63,185,80,.18)':'transparent');
      setP(cells[j],'border-color',today?GREEN:'transparent');
      setP(cells[j],'color',cls==='cur'?WHITE:'rgba(255,255,255,.35)');
      var old=cells[j].querySelectorAll('.cal-dot');for(var q=0;q<old.length;q++)old[q].parentNode.removeChild(old[q]);
      if(has(k)){var dot=el('span');dot.className='cal-dot';text(dot,'•');css(dot,'position:absolute;right:7px;bottom:0;color:'+GREEN+';font-size:16px;line-height:14px;');cells[j].appendChild(dot);}
    }
    text(document.getElementById('cal-title'),state.y+' 年 '+(state.m+1)+' 月');
    var dt=state.selected.split('-'),dow=new Date(+dt[0],+dt[1]-1,+dt[2]).getDay(),events=state.events[state.selected]||[];
    text(document.getElementById('cal-day'),(+dt[1])+' 月 '+(+dt[2])+' 日 · 周'+week[dow]+(state.selected===state.today?'（今天）':''));
    var list=document.getElementById('cal-list');list.innerHTML='';if(!events.length){var empty=el('div');text(empty,'这一天没有事件');css(empty,'height:36px;line-height:36px;text-align:center;color:'+LIGHT+';font-size:15px;');list.appendChild(empty);}else for(var e=0;e<events.length&&e<3;e++){var row=el('div');row.setAttribute('data-del',String(e));css(row,'position:relative;height:30px;line-height:30px;text-align:center;color:'+WHITE+';font-size:15px;border-bottom:1px solid '+BORDER+';cursor:pointer;');var label=el('span');text(label,'•  '+events[e]);css(label,'display:flex;align-items:center;justify-content:center;width:100%;height:30px;');row.appendChild(label);var del=el('span');text(del,'×');css(del,'position:absolute;right:8px;top:0;width:24px;height:30px;line-height:30px;text-align:center;color:'+LIGHT+';');row.appendChild(del);list.appendChild(row);}
  }
  function bind(root){root.addEventListener('click',function(e){var t=e.target,id=t&&t.id;if(id==='cal-prev'){state.m--;if(state.m<0){state.m=11;state.y--;}render();return;}if(id==='cal-next'){state.m++;if(state.m>11){state.m=0;state.y++;}render();return;}if(id==='cal-today'){var d=new Date();state.y=d.getFullYear();state.m=d.getMonth();state.selected=state.today;render();return;}if(id==='cal-add'){add();return;}while(t&&t!==root){if(t.getAttribute){var date=t.getAttribute('data-date'),del=t.getAttribute('data-del');if(date){state.selected=date;render();return;}if(del!==null){remove(+del);return;}}t=t.parentNode;}});var input=document.getElementById('cal-input');if(input)input.addEventListener('keydown',function(e){if(e.key==='Enter')add();});}
  function add(){var input=document.getElementById('cal-input'),s=input&&input.value.trim();if(!s)return;if(!state.events[state.selected])state.events[state.selected]=[];state.events[state.selected].push(s);input.value='';render();}
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

    // Stable fallback: retain DearOreUI Section as the page shell, while the
    // data-heavy calendar is built with explicit absolute DOM geometry. This
    // avoids the client repeatedly re-applying Grid/Stack/Button state styles.
    std::vector<dearoreui::api::DomNode> body;
    body.push_back(dearoreui::api::DomNode{
        .tag   = "div",
        .attrs = {{"id", "cal-root"}},
        // Verified full-screen pattern (stage 7.1): inset 0 instead of the
        // unverified 100vw/100vh viewport units (unsupported units collapse
        // the root to 0x0 and hide the black backdrop).
        .style = "position:fixed;top:0;left:0;right:0;bottom:0;overflow:hidden;",
        .text  = "",
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