# 2026-06-13：Wallpaper Engine 兼容与桌面显示改进

## 变更范围

本次更新实现 Wallpaper Engine 动态壁纸兼容，并根据 6 月桌面显示反馈改进图标渲染、布局和设置页面。

## 主要更新

- 新增 `Wallpaper Engine 动态壁纸兼容模式`。
- 将 Musuka 拟桌面附着到 Explorer 桌面宿主，保留动态壁纸画面。
- 兼容模式运行期间隐藏 Windows 原桌面图标，返回设置或正常退出时恢复。
- 将原 `Wallpaper 模式` 明确命名为 `静态壁纸模式`。
- 仅在静态壁纸模式下显示静态壁纸来源和纯色背景设置。
- 修复纯色背景预览。
- 根据主屏幕尺寸创建拟桌面画布。
- 原始图标优先按左侧紧凑列排列。
- 原始图标与替换图片使用不同默认尺寸。
- 改进透明图片、黑边图标和高分辨率缩放处理。
- 新增 Wallpaper Engine 集成冒烟测试。
- 补充第三方默认图片及素材声明。

## 兼容性说明

- 动态壁纸兼容模式需要 Explorer 桌面宿主窗口可被发现。
- 强制终止 Musuka 后，Explorer 原桌面图标可能需要通过重启 Windows Explorer 恢复。
- 静态壁纸模式仍由 Musuka 拟桌面窗口负责背景和图标绘制。

## 验证

- Wallpaper Engine 动态壁纸保持播放。
- Musuka 替换图标正常显示。
- Explorer 原桌面图标可隐藏并恢复。
- Qt 和当时保留的原生设置页模式名称、选项显示和纯色预览通过冒烟测试。

## 相关提交

- `8e247eb` `feat: add Wallpaper Engine dynamic wallpaper compatibility`
- `71d78d0` `fix: address June desktop feedback`

## 相关文档

- [技术文档](../TECHNICAL.md)
- [主 README](../../README.md)
