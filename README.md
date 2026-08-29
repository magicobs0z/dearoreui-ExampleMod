# DearOreUI 示例模组

把 [DearOreUI](https://github.com/copper-lamp/Dear-OreUI) 作为**前置库**接入 LeviLamina 客户端模组的示例工程，也是一套 **阶梯教程（日历主题）**：同一个日历从静态骨架一步步长成完整全屏日历，顺带学完 DearOreUI 的全部能力。

## 依赖

- [LeviLamina](https://github.com/LiteLDev/LeviLamina) 26.10.x（client）
- [DearOreUI](https://github.com/copper-lamp/Dear-OreUI) v0.1.1（前置，`manifest.json` 已声明依赖）

## 构建

```powershell
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -v -y
```

产物位于 `bin/my-mod/`。

## 安装与使用

1. 先安装 DearOreUI 前置。
2. 将 `bin/my-mod/` 复制到客户端 `mods/` 目录。
3. 编辑模组目录下 `config.json` 的 `"example"` 字段切换课程。
4. 重启游戏（或重进主菜单）生效。**切换课程无需重新编译**。

## 课程文档

- [在你的模组中使用 DearOreUI](https://copper-lamp.github.io/dearoreui-docs/guide/environment)

## 许可证

[CC0-1.0](LICENSE)
