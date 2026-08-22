# 课程 07 - Full Calendar（组件树构建的完整日历，毕业形态）

01–06 全部能力的组合，默认示例。**页面由 DearOreUI Section 壳承载，动态区域采用稳定绝对定位 DOM**。脚本负责：**绝对定位布局、数据填充与中文、事件委托**。**全屏**：黑色 95% 蒙层铺满屏幕，内容矩形四边各留 10%——**没有卡片套子**。

## 包含的能力（对应前课）

| 日历切片 | 前课 | 说明 |
|---|---|---|
| 月历网格（翻页/今天/选日/悬停） | 03 | 42 个 `Button` 组件格子，脚本填日期 |
| 种子事件推送 `calendar.events` | 04 | Ready 时 C++ 推 3 天种子事件（中文） |
| 帧时钟 `calendar.clock` + 跨日刷新 | 05 | 每秒推送；`ymd` 变化自动更新今天描边 |
| 事件标注 + 详情侧栏 | 02/03 | 有事件日绿点（格内数据标记）；选中日 4 个 `ListItem` 事件槽 |
| 本地新增/删除事件 | 03 交互 | `Input` + `Button(添加)` / Enter；条目 `×` 删除 |
| `calendar.init` host method | 06 | 注册保留（演示单 dispatch），脚本**不调用** |

## 组件树与稳定性分工

本课优先保证真机页面完整显示：DearOreUI `Section` 负责页面挂载与生命周期；动态月历、事件列表和输入控件使用已验证稳定的绝对定位 DOM。之前尝试把 42 个日期和事件槽强行放进 `Grid/Stack/Button` 组件树，会被客户端反复应用默认状态样式，出现横向按钮刷屏，因此不再强行使用。

## 组件树（页面=组件，不是手写 DOM）

```
section                       ← 根容器（脚本转 fixed 全屏黑蒙）
├─ grid(7 列)                  ← 7× Text(星期) + 42× Button(日期格)
├─ nav stack(row)              ← Button(‹) · Text(月份标题) · Button(今天) · Button(›)
└─ ev stack(column)            ← 事件区
   ├─ Text(选中日标题)
   ├─ slots stack(column)      ← 4× ListItem[ Text(事件), Button(×) ]
   ├─ row stack(row)           ← Input + Button(添加)
   └─ Text(时钟)
```

- **渲染**：可复用的静态结构优先使用 `Section/Grid/Stack/Button/Text/Input/ListItem`；但本机对动态 Grid/Stack 状态重应用不稳定，因此本版本保留 `Section` 组件壳，其余动态区域采用可靠 DOM fallback。目标是稳定完整显示，而不是为了组件数量牺牲页面。
- **脚本分工**（`kPageScript`）：
  1. 根 `Section` 容器转 `position:fixed; 100vw×100vh; rgba(0,0,0,0.95)`，实际内容矩形四边各留 10%，所有宽度使用脚本像素计算（不使用 `calc()`）；
  2. 因 CSS grid/flex 在本机页面上下文不可靠，组件元素由脚本显式绝对坐标重排（几何仍全屏自适应：列宽=屏宽/7、行高=余高/6）；
  3. 填充日期/事件/时钟数据，组件节点简体中文翻写（组件 label 必须 ASCII——主题字体无 CJK 字形；运行期脚本按 `font-family:YaHei/…` 栈重写文本）；
  4. 交互全部 `addEventListener` 委托（组件状态 click 合成不可靠）。
- **成功标识**：全屏黑蒙、无卡片、组件原版纹理按钮网格 7×6 铺满屏、中文正常、今天绿描边/选中绿底、事件点与列表、增删、时钟跳动、无崩溃。

## 代码位置

- `src/mod/examples/ex07_calendar.h` / `.cpp`（组件树在 `registerAll`，脚本在 `kPageScript`）。

## 改造手法参考（把别的页面迁进本框架）

类继承 `ExampleBase`（`registerAll`/`shutdown`/`name`），`mApi`/`mMod` 来自基类；`mModId` 类内固定；factory 按 config 构造（未知 id 回退 07）。