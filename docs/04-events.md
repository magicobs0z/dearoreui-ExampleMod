# 课程 04 - Events (C++ → JS)：推 today

让 C++ 在**页面就绪**时把"今天"推给日历：月份标题与今天的描边都来自 C++ 数据，而不是页面本地时钟 —— 单向事件契约的第一步。

## 学什么

- `subscribePage(PageSubscriptionOptions{owner, scopes}, PageEvent::Ready, cb)`：页面挂载 + 脚本上下文就绪回调，携带 `PageContextView`（含 `id`）。
- `publishEvent(EventPublishOptions{owner, context, name, payload})`：向指定页面上下文推送 JSON 字符串。
- 页面端 `window.oreui.event.on('calendar.today', cb)`：回调**直接收到 payload 对象**（无需 JSON.parse）。

## 代码位置

- `src/mod/examples/ex04_events.h`：`registerAll` 注册组件 + Ready 订阅；`onReady` 里 `publishEvent`。

## 成功标识

页面就绪后：

- 月份标题/今天描边来自 C++ payload（`y/m/d`），页面本地时钟只是兜底；
- 信息行显示 `push: 1 (from C++)`。

## 切换方法

`config.json` → `"example": "04"`，重启游戏。

## 已知坑

- **payload 契约**：payload 是 ≤ 256KiB 的 JSON 字符串；事件名可含 `.`。回调收到的是对象，别二次 parse。
- **订阅时机**：Ready 在 UI mount 后触发；若 UI 注册失败回调永远不来 —— 先看 `registerComponent` 的 error。
- **scope 语义**：订阅 scope 为空、含目标页、或含 `Any` 都会被通知；`{Any}` 是本客户端正确的通配。
- 单向数据流：想让 JS 请求 C++ 数据看课程 06（单 dispatch）；本课刻意只做 C++→JS。