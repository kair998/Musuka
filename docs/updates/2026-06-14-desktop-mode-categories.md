# 2026-06-14：桌面模式分类与静态壁纸兼容模式

## 变更范围

本次更新重新整理桌面运行模式，新增直接在 Windows 桌面静态壁纸上运行的兼容模式，并统一兼容模式的桌面集成实现。

## 主要更新

- 将原“静态壁纸模式”改名为“静态壁纸拟桌面模式”。
- 新增“桌面静态壁纸兼容模式”。
- 将“桌面静态壁纸兼容模式”和“Wallpaper Engine 动态壁纸兼容模式”归入设置页的“兼容模式”分类。
- 两种兼容模式共用透明桌面图标层，直接保留底层壁纸画面，仅绘制 Musuka 图标。
- 兼容模式运行期间隐藏 Explorer 原桌面图标，返回设置或正常退出时恢复。
- 静态壁纸来源和纯色背景设置仅在静态壁纸拟桌面模式下显示。

## 配置兼容

- 新配置使用 `static_wallpaper_virtual_desktop`、`desktop_static_wallpaper_compatibility` 和 `wallpaper_engine_compatibility`。
- 旧值 `wallpaper` 自动迁移为静态壁纸拟桌面模式。
- 旧值 `wallpaper_engine` 自动迁移为 Wallpaper Engine 动态壁纸兼容模式。

## 兼容性说明

- 两种兼容模式都需要能够发现 Explorer 桌面宿主窗口。
- 若 Musuka 在兼容模式下被强制终止，可能需要重启 Windows Explorer 恢复原桌面图标。
- 静态壁纸拟桌面模式仍由 Musuka 绘制系统静态壁纸或纯色背景，不修改 Explorer 原桌面图标状态。

## 验证

- `build.bat` 通过。
- `ctest --test-dir build-nmake --output-on-failure` 通过。
- 冒烟测试覆盖两种兼容模式的桌面附着、透明色键、替换图标绘制、Explorer 图标隐藏与恢复。
- 冒烟测试覆盖设置页模式名称、兼容模式分组、选项显隐、新配置值序列化和旧配置值迁移。
