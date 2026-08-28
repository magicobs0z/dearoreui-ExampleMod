(function () {
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
