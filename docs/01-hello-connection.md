# 课程 01 - Hello Connection

日历之旅的起点：最小可运行模组，什么都不显示，把 DearOreUI 的**连接契约**跑通。后面 06 课都建立在这条链路上。

## 学什么

- 前置库不链接 import lib：通过 DearOreUI.dll 导出的纯 C 桥 `DearOreUI_QueryApi` 拿到 `IDearOreUIApi*`（见 `MyMod::connectDearOreUI`）。
- `registerMod(ModManifest)` 注册模组身份 —— **之后所有注册（UI/host method/订阅）都以它为 owner 校验**。
- `getInfo()` / `getProtocolVersion()` / `isReady()` 探测运行时状态。

## 代码位置

- `src/mod/examples/ex01_hello_connection.h`（约 45 行，全部逻辑在 `registerAll`）
- 桥接入口：`src/mod/MyMod.cpp`（`loadBridge` + 延迟重试）

## 成功标识

主机/LE 控制台日志出现：

```
[example.hello] connected: protocol=v1 ready=true minecraft=... oreui=... coherent=...
```

没有这一步，后面的示例都不会工作 —— 这是第一个要排的坑。

## 切换方法

`config.json` → `"example": "01"`，重启游戏。

## 已知坑

- **协议版本**：`queryApi(1)` 的 `1` 是桥协议版本。`DearOreUIBridge_VersionMismatch` 说明 DearOreUI 版本与模组编译时接口不一致（头文件需匹配）。
- **启动顺序**：DearOreUI 是依赖模组（`manifest.json` 的 dependencies 声明），正常先于本模组启用；防御性重试线程会在就绪后把 API 交还主线程。若日志出现 `waiting for it (retry loop)`，多半是依赖顺序或 DearOreUI 未装载。
- `ApiInfo` 字段是 `minecraftVersion/oreuiVersion/coherentVersion/ready` 等，**没有 displayName**（编译时注意）。