# 2026-06-13：仅 Qt 构建与可运行发布包

## 变更范围

本次更新统一了 Musuka 的 UI、构建、测试和发布流程，并建立了项目技术文档与更新记录体系。

## 主要更新

- 删除原生 Win32 Settings 界面及相关条件编译分支。
- Qt 6 Widgets 成为唯一设置界面。
- 使用 `QApplication::exec()` 作为唯一应用 UI 主循环。
- 删除独立的 `build-qt.bat`。
- `build.bat` 固定将 Qt 版本构建到 `build-nmake`。
- `build.bat` 自动部署 Qt DLL、插件和 MSVC x64 CRT DLL。
- `package.bat` 生成可直接运行的动态 Qt 发布包。
- release 包含 `default_image`、`data`、Qt 运行库、平台插件和必要 MSVC CRT DLL。
- 打包前严格清理旧 release；目录被运行中程序占用时明确失败。
- 打包后验证关键 Qt 和 MSVC 运行库存在。
- Qt 冒烟测试替代原生 Settings 控件测试。
- 修复 Qt 纯色预览框应用样式后不再保持正方形的问题。

## 兼容性说明

- 项目不再支持原生 Win32 Settings 构建。
- 项目只支持 Qt 6、MSVC 2022 和 NMake Makefiles。
- Qt 使用动态链接，`musuka.exe` 不能脱离 release 中的 Qt DLL 和 `platforms/qwindows.dll` 单独分发。
- `build-nmake-qt` 已废弃，统一使用 `build-nmake`。

## 验证

- `build.bat` 构建通过。
- `wallpaper_engine_smoke` 通过。
- `package.bat` 打包通过。
- 从 `release` 直接启动 `musuka.exe`，Qt Settings 窗口正常显示并退出。

## 相关文档

- [技术文档](../TECHNICAL.md)
- [主 README](../../README.md)
- [安全审计](../SECURITY_AUDIT.md)
