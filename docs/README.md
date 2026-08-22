# DearOreUI 示例模组 · 阶梯教程（日历主题）

把 DearOreUI 作为**前置库**接入你自己的 LeviLamina 客户端模组，从零开始逐步学。
本模组 = **同一个日历产品的 7 个构建阶段**（01→07 难度递增）：每课在日历上新增一个真实可见的切片，跟完全部课程你就得到了一个能用的精美日历，同时学完 DearOreUI 全部六项能力。同一时间只运行一课，用 `config.json` 切换。

## 怎么用

1. 构建并部署本模组（依赖 `DearOreUI` 前置，`manifest.json` 已声明）。
2. 编辑模组目录下 `config.json`：`"example": "07"`（默认，毕业完整日历）。
   - `"01"`…`"07"` 选择对应课程；未知值回退 `07` 并在日志告警。
3. 重启游戏（或重新进入主菜单）即可看到效果。**切换课程不需要重新编译**。

## 阶梯（一幅日历如何一步步长成）

| # | 课程 | 日历切片 | 教的能力 / API | 成功标识（真机） |
|---|---|---|---|---|
| [01](01-hello-connection.md) | Hello Connection | （无 UI，日志验证起步） | C ABI 桥 + `registerMod` / `getInfo` / `isReady` | 日志 `[example.hello] connected: protocol=v1 ready=true` |
| [02](02-static-component.md) | Static Component | 日历**静态骨架**卡片 | 声明式组件渲染（`ComponentSpec` / 34 种 `ComponentKind`） | 主菜单右上角出现月历标题 + 星期表头静态卡片 |
| [03](03-page-script.md) | Page Script | **月历网格可翻页**（纯 JS，绝对定位） | script 注入（mount 后 ExecuteScript）+ DOM/CSS + click | 月视图网格对位正确、今天描边、‹/› 翻页、点选日期 |
| [04](04-events.md) | Events (C++→JS) | 标题/今天由 C++ **推 today** 初始化 | `subscribePage(Ready)` + `publishEvent` + `oreui.event.on` | 月份标题与今天来自 C++ 推送，"push: 1 (from C++)" |
| [05](05-frame-data.md) | Frame Data | **时钟条 + 跨日自动刷新** | `IFrameApi::subscribeFrame`（唯一可靠周期源） | 底部时钟每秒跳动、跨午夜今天高亮自动更新 |
| [06](06-host-method.md) | Host Method (JS→C++) | **init 拉种子事件**（默认关闭/实验） | `registerHostMethod` + 页面 `oreui.host.call` 单 dispatch | init 响应上屏 + JS 心跳继续（验证 facet 通道） |
| [07](07-calendar.md) | Full Calendar | **全屏精美日历**（黑80%蒙层+模糊，屏幕居中） | 全能力组合 + 事件标注/详情 + 本地增删 + 四色规范 | 全屏居中月历、中文正常、今天/选日/事件/时钟齐全 |

## 学习路径建议

- 新人：按 01→07 顺序，每课读源码注释 + 对应页，真机看过效果再前进。
- 只想抄接口：直接看 [07](07-calendar.md)（完整工程，含全屏布局与四色规范）。
- 每页的「已知坑」来自 DearOreUI 客户端真机实证（注入通道唯一性、帧服务唯一周期源、绑定通道不可用、`{Any}` scope、字体与 CJK、payload 契约、单 dispatch、卸载期生命周期、**CSS Grid/Flex 不可靠 → 绝对定位布局**）。遇到诡异行为先查对应陷阱。

## 工程结构

```
src/mod/
  MyMod.{h,cpp}                入口：C ABI 桥 + config 读取 + 工厂驱动
  examples/
    ExampleBase.h              示例抽象基类（registerAll/shutdown/name）
    ExampleFactory.{h,cpp}     config → 示例实例（未知 id 回退 07）
    ex01_hello_connection.h    连接契约（日志）
    ex02_static_component.h    日历静态骨架（声明式组件）
    ex03_page_script.h         月历网格（页面脚本）
    ex04_events.h              推 today（事件推送）
    ex05_frame_data.h          时钟条（帧服务）
    ex06_host_method.h         init 种子事件（facet 单 dispatch，默认关闭）
    ex07_calendar.{h,cpp}      完整日历（毕业形态）
bin/my-mod/config.json         {"example":"07"}
```