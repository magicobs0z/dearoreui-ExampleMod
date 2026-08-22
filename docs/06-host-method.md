# 课程 06 - Host Method (JS → C++)【实验，默认关闭】

让日历页面**主动向 C++ 要数据**：一次 `host.call('calendar.init')` 拉回批量种子事件，渲染成格子绿点 —— JS→C++ 往返闭环。

> ⚠️ 本课**默认关闭**（`config.json` 显式写 `"06"` 才启用）。它是能力演示/实验，不是 07 的默认路径。

## 学什么

- `registerHostMethod(owner, HostMethodManifest{name, pageScopes, permissions}, std::make_shared<IHostMethod>())`；
- `IHostMethod` 三件套：`name()` / `requiredPermission()` / `execute(ContextId, args)` → `Result<std::string>`；
- 页面端 `window.oreui.host.call('calendar.init', args).then(res => …)`，响应经 facet 回传、promise 兑现。

## 代码位置

- `src/mod/examples/ex06_host_method.h`：`InitMethod` + 页面脚本 boot 里单次 dispatch。

## 成功标识

卡片底部信息行显示 `init: loaded 4 event(s) via facet`，今天/明天格子上出现绿点；**下方的 `js: N` 心跳必须持续跳动** —— 证明页面 JS 在 facet dispatch 后存活（本课存在的核心目的）。

## 已知坑（本客户端实证）

- **绑定通道不可用，facet 通道健康**：`RegisterForEvent` / `BindCall` 通道触发游戏自身未处理异常（msxml6，`0x40080201` 系）—— 别用。页面 JS→C++ 只能走 facet。历史上"dispatch 后 JS 死"是绑定通道的误判 —— 本课就是用来亲自复核的。
- **单 dispatch**：每个视图只有一次 dispatch 槽（第二次 `ViewDispatchAlreadyUsed`）。把它当"初始快照批量拉取"，之后数据一律事件推送（04/05）。
- `execute` 返回 `Result<std::string>`，失败以 reject 回到 JS `catch`。
- 页面 boot 与 dispatch 并发：等 DOM 就绪再发（本课 boot 轮询容器）。