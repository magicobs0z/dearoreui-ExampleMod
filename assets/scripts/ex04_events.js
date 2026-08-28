(function () {
  // Absolute-positioned month grid kernel (same geometry/style as lesson 03).
  var PAD = 6, CELL_W = 38, CELL_H = 34, TITLE_H = 34, HEAD_H = 22, FIX_H = 20;
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
    // Lesson 04: caption/today initialized from the C++ push (fallback above).
    window.oreui.event.on('calendar.today', function (payload) {
      var p = payload || {};
      if (p.y && p.m && p.d) {
        state.today = ymdStr(p.y, p.m - 1, p.d);
        state.viewY = p.y; state.viewM = p.m - 1;
      }
      var info = $('cal-info');
      if (info) info.textContent = 'push: ' + (p.pushes || 0) + ' (from C++)';
      renderCalendar();
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
      '<div id="cal-info" style="position:absolute;left:' + PAD + 'px;top:' + (gridTop() + 6 * CELL_H + 6) + 'px;font-family:' + FONT + ';font-size:12px;color:#d0d7de;">waiting for push...</div>';
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
        var info = $('cal-info');
        if (info) info.textContent = 'selected: ' + state.selected;
        renderCalendar();
      }
    });
  }
  boot();
})();
