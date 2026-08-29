(function(){
  // Lesson 04: Calendar Demo (graduation project).
  // Polished centered-panel calendar with all features:
  //   * Month grid with navigation, select, today highlight
  //   * C++->JS seed events (green dots)
  //   * Frame-driven clock
  //   * JS->C++ host method call (calendar.init)
  //   * Event add/delete (local JS-side store)
  //   * Modern visual polish
  // Size adaptation (Docs/DearOreUI-日历多分辨率尺寸适配-需求架构执行.md):
  //   Panel + all geometry scale with the viewport via S (no limits), so the
  //   calendar stays centered and readable on any resolution. sx() applies S.
  var PW=420, PH=560, PAD=16, FONT="'Microsoft YaHei','SimHei','Noto Sans SC','Segoe UI',sans-serif";
  var ACCENT='#3fb950', WHITE='#ffffff', LIGHT='#d0d7de', DARK='#161b22', PANEL='#21262d', BORDER='#30363d';
  var S=Math.min(window.innerWidth/1920, window.innerHeight/1080);
  function sx(n){return Math.round(n*S);}
  var state={y:0,m:0,today:'',selected:'',events:{}};
  var week=['日','一','二','三','四','五','六'];
  function pad(n){return n<10?'0'+n:''+n;}
  function key(y,m,d){return y+'-'+pad(m+1)+'-'+pad(d);}
  function $(id){return document.getElementById(id);}
  function hasEv(k){return state.events&&Object.prototype.hasOwnProperty.call(state.events,k)&&state.events[k].length>0;}
  var tries=0;
  function boot(){
    tries++; var root=$('cal-root');
    if(!root||!window.oreui){if(tries<60)setTimeout(boot,50);return;}
    var d=new Date(); state.y=d.getFullYear();state.m=d.getMonth();
    state.today=key(state.y,state.m,d.getDate());state.selected=state.today;
    buildLayout(root); render(); bind(root);
    // 分辨率/视口变化（App 拖拽或切分辨率）→ 按新视口重新构图（防抖一次收敛）。
    // 仅重建几何 DOM，事件监听、facet、Oreui 事件推送均不受影响。
    var rt=null;
    window.addEventListener('resize',function(){
      if(rt)clearTimeout(rt);
      rt=setTimeout(function(){var r=$('cal-root');if(r){buildLayout(r);render();}},150);
    });
    // Facet: pull authoritative today + events from C++
    try{
      window.oreui.host.call('calendar.init',{want:['today','events']}).then(function(res){
        try{var p=JSON.parse(res);if(p&&typeof p.today==='string')state.today=p.today;state.events=p&&p.events?p.events:{};}catch(e){state.events={};}
        render();
      }).catch(function(){});
    }catch(e){}
    // C++->JS pushes
    window.oreui.event.on('calendar.events',function(p){if(p&&p.events)state.events=p.events;render();});
    window.oreui.event.on('calendar.clock',function(p){
      var c=$('cal-clock');if(c)c.textContent=pad(p.h||0)+':'+pad(p.mi||0)+':'+pad(p.s||0);
      var k=key(p.y||0,(p.m||1)-1,p.d||1);
      if(k!==state.today){var old=state.today;state.today=k;if(state.selected===old){state.selected=k;state.y=p.y||state.y;state.m=(p.m||1)-1;}render();}
    });
  }
  function buildLayout(root){
    root.innerHTML='';
    var W=window.innerWidth,H=window.innerHeight;
    var pw=sx(PW),ph=sx(PH),pad=sx(PAD),r12=sx(12),r6=sx(6);
    var lx=Math.floor((W-pw)/2),ly=Math.floor((H-ph)/2);
    // Backdrop
    var bd=document.createElement('div');
    bd.style.cssText='position:absolute;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);';
    root.appendChild(bd);
    // Panel
    var p=document.createElement('div');
    p.id='cal-panel';
    p.style.cssText='position:absolute;left:'+lx+'px;top:'+ly+'px;width:'+pw+'px;height:'+ph+'px;display:flex;flex-direction:column;box-sizing:border-box;padding:'+pad+'px;background:'+DARK+';border:1px solid '+BORDER+';border-radius:'+r12+'px;font-family:'+FONT+';color:'+WHITE+';user-select:none;';
    root.appendChild(p);
    // Header
    var hdr=document.createElement('div');
    hdr.style.cssText='flex:none;display:flex;flex-direction:row;align-items:center;height:'+sx(48)+'px;margin:0;';
    p.appendChild(hdr);
    var title=document.createElement('div');
    title.id='cal-title';title.style.cssText='flex:1;font-size:'+sx(18)+'px;font-weight:700;color:'+WHITE+';margin:0;';
    hdr.appendChild(title);
    var nav=document.createElement('div');
    nav.style.cssText='flex:none;display:flex;flex-direction:row;margin:0;';
    hdr.appendChild(nav);
    function btn(id,label,w,m){var b=document.createElement('div');b.id=id;b.textContent=label;b.style.cssText='flex:none;width:'+sx(w)+'px;height:'+sx(32)+'px;display:flex;align-items:center;justify-content:center;font-size:'+sx(14)+'px;color:'+LIGHT+';background:'+PANEL+';border:1px solid '+BORDER+';border-radius:'+r6+'px;cursor:pointer;margin-left:'+sx(m||8)+'px;';nav.appendChild(b);}
    btn('cal-prev','‹',36,0);btn('cal-today','今',44);btn('cal-next','›',36);
    // Week header
    var wk=document.createElement('div');
    wk.style.cssText='flex:none;display:flex;flex-direction:row;height:'+sx(28)+'px;margin:'+sx(4)+'px 0 0 0;';
    p.appendChild(wk);
    for(var i=0;i<7;i++){
      var wh=document.createElement('div');
      wh.textContent=week[i];wh.style.cssText='flex:1;height:'+sx(28)+'px;display:flex;align-items:center;justify-content:center;font-size:'+sx(13)+'px;color:'+LIGHT+';margin:0;';
      wk.appendChild(wh);
    }
    // Grid
    var grid=document.createElement('div');
    grid.id='cal-grid';grid.style.cssText='flex:1;display:flex;flex-direction:column;margin:'+sx(4)+'px 0 0 0;';
    p.appendChild(grid);
    for(var r=0;r<6;r++){
      var row=document.createElement('div');
      row.style.cssText='flex:1;display:flex;flex-direction:row;margin-top:'+(r>0?sx(4)+'px':'0')+';';
      grid.appendChild(row);
      for(var c=0;c<7;c++){
        var cell=document.createElement('div');
        cell.setAttribute('data-index',String(r*7+c));
        cell.style.cssText='flex:1;display:flex;align-items:center;justify-content:center;font-size:'+sx(15)+'px;cursor:pointer;border-radius:'+r6+'px;color:'+WHITE+';margin:0;margin-right:'+(c<6?sx(4)+'px':'0')+';';
        row.appendChild(cell);
      }
    }
    // Bottom: event list + input + clock
    var bot=document.createElement('div');
    bot.id='cal-bottom';bot.style.cssText='flex:none;height:'+sx(200)+'px;display:flex;flex-direction:column;margin:'+sx(4)+'px 0 0 0;border-top:1px solid '+BORDER+';';
    // Selected date header
    var selHdr=document.createElement('div');
    selHdr.id='cal-sel-header';selHdr.style.cssText='flex:none;height:'+sx(32)+'px;display:flex;align-items:center;font-size:'+sx(14)+'px;font-weight:600;color:'+WHITE+';padding:0 '+sx(4)+'px;margin:0;';
    bot.appendChild(selHdr);
    // Event list
    var list=document.createElement('div');
    list.id='cal-list';list.style.cssText='flex:1;overflow:hidden;padding:0 '+sx(4)+'px;margin:0;';
    bot.appendChild(list);
    // Input row
    var rowInp=document.createElement('div');
    rowInp.style.cssText='flex:none;display:flex;flex-direction:row;align-items:center;height:'+sx(40)+'px;margin:'+sx(4)+'px 0 0 0;';
    var inp=document.createElement('input');
    inp.id='cal-input';inp.setAttribute('placeholder','添加事件...');
    inp.style.cssText='flex:1;height:'+sx(36)+'px;padding:0 '+sx(10)+'px;box-sizing:border-box;font-family:'+FONT+';font-size:'+sx(14)+'px;color:'+WHITE+';background:'+PANEL+';border:1px solid '+BORDER+';border-radius:'+r6+'px;outline:none;margin:0;';
    rowInp.appendChild(inp);
    var addBtn=document.createElement('div');
    addBtn.id='cal-add';addBtn.textContent='+';
    addBtn.style.cssText='flex:none;width:'+sx(40)+'px;height:'+sx(36)+'px;display:flex;align-items:center;justify-content:center;font-size:'+sx(20)+'px;font-weight:700;color:'+WHITE+';background:'+ACCENT+';border:none;border-radius:'+r6+'px;cursor:pointer;margin-left:'+sx(8)+'px;';
    rowInp.appendChild(addBtn);
    bot.appendChild(rowInp);
    // Clock
    var clock=document.createElement('div');
    clock.id='cal-clock';clock.style.cssText='flex:none;height:'+sx(24)+'px;display:flex;align-items:center;justify-content:flex-end;font-size:'+sx(13)+'px;font-weight:600;color:'+ACCENT+';padding:0 '+sx(4)+'px;margin:0;';clock.textContent='--:--:--';
    bot.appendChild(clock);
    p.appendChild(bot);
  }
  function render(){
    var title=$('cal-title');if(title)title.textContent=state.y+' 年 '+(state.m+1)+' 月';
    var first=new Date(state.y,state.m,1),lead=first.getDay(),days=new Date(state.y,state.m+1,0).getDate(),prev=new Date(state.y,state.m,0).getDate();
    var grid=$('cal-grid');if(!grid)return;
    var cells=grid.querySelectorAll('[data-index]');
    for(var i=0;i<42&&i<cells.length;i++){
      var m=state.m,y=state.y,dn,cls='cur';
      if(i<lead){cls='prev';m--;dn=prev-lead+1+i;}
      else if(i>=lead+days){cls='next';m++;dn=i-lead-days+1;}
      else dn=i-lead+1;
      var k=key(y,m,dn),sel=k===state.selected,today=k===state.today;
      cells[i].textContent=dn;
      cells[i].setAttribute('data-date',k);
      var bg='transparent',ring='',color=cls==='cur'?WHITE:'rgba(255,255,255,0.35)';
      if(sel){bg=ACCENT;color=WHITE;}
      else if(today){bg='rgba(63,185,80,0.12)';ring='inset 0 0 0 1px '+ACCENT;color=ACCENT;}
      cells[i].style.background=bg;
      cells[i].style.boxShadow=ring;
      cells[i].style.color=color;
      // Event dots (in-place update)
      var dot=cells[i].getElementsByClassName('cal-dot')[0];
      if(hasEv(k)&&!dot){
        dot=document.createElement('div');dot.className='cal-dot';
        dot.style.cssText='position:absolute;bottom:'+sx(2)+'px;left:50%;width:'+sx(4)+'px;height:'+sx(4)+'px;margin-left:'+(-sx(2))+'px;border-radius:50%;background:'+ACCENT+';';
        cells[i].appendChild(dot);
      }else if(!hasEv(k)&&dot){
        cells[i].removeChild(dot);
      }
    }
    // Bottom: selected date + event list
    var selHdr=$('cal-sel-header');if(selHdr)selHdr.textContent=state.selected;
    var list=$('cal-list');if(!list)return;
    var evs=state.events[state.selected]||[];
    list.innerHTML='';
    if(evs.length===0){
      var empty=document.createElement('div');
      empty.textContent='这一天没有事件';
      empty.style.cssText='height:'+sx(32)+'px;display:flex;align-items:center;justify-content:center;font-size:'+sx(13)+'px;color:'+LIGHT+';';
      list.appendChild(empty);
    }else{
      for(var e=0;e<evs.length&&e<5;e++){
        var row=document.createElement('div');
        row.style.cssText='display:flex;flex-direction:row;align-items:center;height:'+sx(28)+'px;margin:0;';
        var label=document.createElement('div');
        label.textContent='• '+evs[e];
        label.style.cssText='flex:1;font-size:'+sx(13)+'px;color:'+WHITE+';overflow:hidden;text-overflow:ellipsis;white-space:nowrap;';
        row.appendChild(label);
        var del=document.createElement('div');
        del.setAttribute('data-del',String(e));
        del.textContent='×';
        del.style.cssText='flex:none;width:'+sx(24)+'px;height:'+sx(28)+'px;display:flex;align-items:center;justify-content:center;font-size:'+sx(16)+'px;color:'+LIGHT+';cursor:pointer;';
        row.appendChild(del);
        list.appendChild(row);
      }
    }
  }
  function addEvent(){
    var inp=$('cal-input'),s=inp&&inp.value.trim();
    if(!s)return;
    if(!state.events[state.selected])state.events[state.selected]=[];
    state.events[state.selected].push(s);inp.value='';render();
  }
  function removeEvent(i){
    var a=state.events[state.selected];if(!a)return;
    a.splice(i,1);if(!a.length)delete state.events[state.selected];render();
  }
  function bind(root){
    root.addEventListener('click',function(e){
      var t=e.target||e.srcElement,id=t&&t.id;
      if(id==='cal-prev'){state.m--;if(state.m<0){state.m=11;state.y--;}render();return;}
      if(id==='cal-next'){state.m++;if(state.m>11){state.m=0;state.y++;}render();return;}
      if(id==='cal-today'){var d=new Date();state.y=d.getFullYear();state.m=d.getMonth();state.selected=state.today;render();return;}
      if(id==='cal-add'){addEvent();return;}
      // Date cell selection
      if(t&&t.getAttribute&&t.getAttribute('data-date')){state.selected=t.getAttribute('data-date');render();return;}
      // Event deletion
      while(t&&t!==root){
        if(t.getAttribute&&t.getAttribute('data-del')!==null){removeEvent(+t.getAttribute('data-del'));return;}
        t=t.parentNode;
      }
    });
    var inp=$('cal-input');if(inp)inp.addEventListener('keydown',function(e){if(e.key==='Enter')addEvent();});
  }
  boot();
})();