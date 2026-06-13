# 2026-06-13：当前开发版本更新总结

## 概述

当前开发版本完成了 Musuka 从双设置界面架构向仅 Qt 架构的迁移，统一了构建与发布流程，并重新组织了项目文档。此前完成的 Wallpaper Engine 动态壁纸兼容、桌面图标布局和图片共享机制继续保留。

## 应用架构

- 删除原生 Win32 Settings 界面及其源码。
- Qt 6 Widgets 成为唯一设置界面。
- `QApplication::exec()` 成为唯一应用 UI 主循环。
- 拟桌面窗口继续使用 Win32、Shell API 与 GDI+，用于 Explorer 和 Wallpaper Engine 集成。
- 删除 `MUSUKA_USE_QT` 可选构建分支，不再支持非 Qt 构建。
- 修复 Qt 纯色背景预览框受样式影响后无法保持正方形的问题。

## 构建与发布

- 删除独立的 `build-qt.bat`。
- `build.bat` 统一将 Qt Release 版本构建到 `build-nmake`。
- 构建时自动部署 Qt DLL、Qt 插件和 MSVC x64 CRT DLL。
- `package.bat` 会先严格清理旧 `release`，目录被运行中程序占用时立即失败。
- release 包含：
  - `musuka.exe`
  - `default_image`
  - `data`
  - Qt 动态运行库和插件
  - MSVC x64 CRT DLL
  - 必要 DirectX 辅助 DLL
- 打包后检查关键 Qt 和 MSVC 运行库，避免生成无法直接启动的发布包。

## 桌面与图片机制

- 保留 Wallpaper Engine 动态壁纸兼容模式。
- 兼容模式附着到 Explorer 桌面宿主，隐藏并在退出时恢复 Windows 原桌面图标。
- 原始图标使用紧凑左侧布局和 32 像素默认尺寸。
- 替换图片使用 96 像素默认尺寸，并保留更大的视觉留白。
- 异常透明留白过大的原始图标会在渲染时条件裁边，避免实际图案缩成极小像素。
- 自动排列只调整位置，不再自动缩小并保存图标尺寸。
- 左键拖动框选会实时高亮选中对象，并显示更清晰的选区边框。
- `default_image` 继续作为所有对象共享的独立候选图片目录。
- 默认图片不会复制到每个对象目录，也不会批量写入每个对象配置。

## 测试

- `wallpaper_engine_smoke` 已切换为 Qt 设置页测试。
- 当前测试覆盖 Wallpaper Engine 桌面附着、透明渲染、Explorer 图标隐藏与恢复、Qt 模式名称、静态壁纸选项、纯色预览、异常透明画布裁边和自动排列尺寸保持。
- 构建、冒烟测试、可移植打包及 release 直接启动均已验证。

## 文档

- 主 README 精简为用户说明和文档入口。
- 新增技术文档，集中维护项目结构、运行架构、数据模型、图片机制、构建、打包和测试细节。
- 新增 `docs/updates` 更新记录目录和索引。
- 更新安全审计中的 Qt 源码引用与自动化测试现状。

## 兼容性影响

- 开发环境必须安装 Qt 6 MSVC 2022 64-bit。
- 项目仅支持 MSVC 2022 + NMake Makefiles。
- `build-nmake-qt` 和原生 Win32 Settings 构建已废弃。
- Qt 使用动态链接，不能只分发单独的 `musuka.exe`；必须保留 release 中的 DLL 和插件目录。

## 验证结果

- `build.bat`：通过。
- `ctest --test-dir build-nmake --output-on-failure`：通过。
- `package.bat`：通过。
- `release/musuka.exe`：直接启动、显示 Qt Settings 并正常退出。
- Markdown 本地链接检查：通过。
- `git diff --check`：通过。

## 详细记录

- [仅 Qt 构建与可运行发布包](2026-06-13-qt-only-build.md)
- [Wallpaper Engine 兼容与桌面显示改进](2026-06-13-wallpaper-engine-and-desktop.md)
- [图标预览、尺寸与框选反馈修复](2026-06-13-icon-preview-size-and-selection.md)
- [技术文档](../TECHNICAL.md)
- [安全审计](../SECURITY_AUDIT.md)
