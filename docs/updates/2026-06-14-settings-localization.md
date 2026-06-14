# 2026-06-14：Settings 中英日多语言切换

## 变更范围

本次更新为 Musuka Settings 新增中文、English、日本語三语言切换，并建立集中维护的 Settings 文案层。

## 主要更新

- 在 Settings 底部导航栏新增语言选择器，所有页面均可使用。
- 切换语言后立即重建当前 Settings 页面，无需重启程序。
- 切换时保留当前页面、对象选择和已有配置状态。
- 本地化三步页面、桌面模式名称、导航按钮、对象与候选图状态、预览状态及导入相关提示。
- 本地化 Settings 阶段显示的常见扫描、图片导入和配置保存错误消息。
- “此电脑”“回收站”和原始图标在 Settings 对象列表中按当前语言显示。

## 配置兼容

- `data/config.json` 新增 `settings_language`。
- 支持 `zh_CN`、`en`、`ja`。
- 旧配置未包含语言值时默认使用中文。

## 验证

- `build.bat` 通过。
- `ctest --test-dir build-nmake --output-on-failure` 通过。
- 冒烟测试覆盖中文、English、日本語即时切换。
- 冒烟测试覆盖语言配置值序列化、主要模式名称翻译和常见提示消息翻译。
