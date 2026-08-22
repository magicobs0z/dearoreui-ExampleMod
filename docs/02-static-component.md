# 课程 02 - Static Component（日历静态骨架）

用**声明式组件**画出日历的第一块拼图：静态的月历外壳（标题 + 星期表头），证明你不手搓 HTML 也能渲染 UI 骨架。
真正的月历网格在课程 03 由页面脚本画出。

## 学什么

- `ComponentSpec` 树：`kind`（34 种 `ComponentKind`：Panel/Button/Text/Card/Grid/...）+ `label` + `style` + `children`。
- `Grid` 组件 + `columns` 排 7 列星期表头。
- `registerComponent(owner, UiManifest, spec)` 注册；句柄可 `unregisterUi` 注销。
- `UiManifest` 关键字段：`pageScopes`（哪些页面显示）、`anchor`（吸附位置）、`pointerEvents`、`containerId`（`makeUiContainerId` 生成）、`fingerprint`。

## 代码位置

- `src/mod/examples/ex02_static_component.h` —— `registerAll` 里从叶子到根拼 ComponentSpec 树。

## 成功标识

主菜单右上角出现静态卡片：

```
┌─ Calendar - Static Component ─────┐
│ AUGUST 2026 (static skeleton)     │
│ SUN MON TUE WED THU FRI SAT       │
│ Lesson 03 draws the live month    │
│ grid via page script.             │
└───────────────────────────────────┘
```

## 切换方法

`config.json` → `"example": "02"`，重启游戏。

## 已知坑

- **CJK 字体**：组件静态 `label` 必须 ASCII —— 原版渲染字体没有中文字形，中文 label 渲染成碎块（所以这课的表头是 `SUN/MON/...` 而不是 `日/一/...`）。动态文本（页面脚本写入）用 `font-family:Noto Sans` 可正常中文（课程 03 起）。
- **pageScopes 通配**：`{PageScope::Any}` 是正确的"所有页面"写法（历史 bug：Any 曾无法匹配，已修）。空 scope 列表不可靠。
- `containerId` 用 `makeUiContainerId(modNamespace, kind, id)` 生成，不要手拼。
- `ComponentSpec` 没有 `size` 字段；排版交给 `Grid`/`Stack` 等组合组件。