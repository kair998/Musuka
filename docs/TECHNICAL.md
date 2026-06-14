# Musuka 技术文档

本文面向参与 Musuka 开发、构建、测试和发布的维护者。用户使用说明见[主 README](../README.md)，性能结果见[性能优化报告](PERFORMANCE_OPTIMIZATION.md)，更新历史见[更新记录](updates/README.md)，安全相关细节见[安全审计](SECURITY_AUDIT.md)。

## 技术栈

- 语言：C++17
- 设置界面：Qt 6 Widgets
- 桌面图标层：Win32 API、Shell API、GDI+
- 构建系统：CMake 3.20+、MSVC 2022、NMake Makefiles
- 配置格式：项目内置 JSON 解析与序列化
- 目标平台：Windows 10/11 x64

Musuka 仅支持 Qt 设置界面。Qt 负责设置窗口、控件和应用主事件循环；桌面图标层仍使用 Win32 与 Explorer、Wallpaper Engine 和 Windows Shell 集成。

## 项目结构

```text
Musuka\
  assets\                  应用图标等源资产
  default_image\           所有桌面对象共享的内置候选图片
  docs\
    TECHNICAL.md           本技术文档
    SECURITY_AUDIT.md      安全审计
    updates\               按更新维护的变更记录
  src\                     主程序源码
  tests\                   自动化冒烟测试
  build.bat                统一 Qt 构建脚本
  package.bat              可直接运行的发布包生成脚本
  CMakeLists.txt           构建目标与依赖定义
  build-nmake\             本地 Qt 构建目录，不提交
  release\                 本地发布目录，不提交
```

### 核心模块

| 模块 | 职责 |
|---|---|
| `main.cpp` | 初始化 `QApplication`、COM、GDI+，持有 Qt 设置窗口并启动应用 |
| `App.cpp/.h` | 管理配置、拟桌面窗口，并协调设置窗口和应用生命周期 |
| `QtSettingsWindow.cpp/.h` | 三步设置界面、对象配置、图片导入与模式选择 |
| `SettingsLocalization.cpp/.h` | Settings 中英日文案、语言名称和提示消息本地化 |
| `DesktopScanner.cpp/.h` | 扫描桌面对象、生成稳定对象 ID、提取原始图标 |
| `DesktopWindow.cpp/.h` | 桌面图标层绘制、命中测试、拖动、缩放、打开对象和桌面兼容模式集成 |
| `Models.h` | 应用配置、桌面对象和候选图片数据模型 |
| `ConfigStore.cpp/.h` | `data/config.json` 的加载、迁移、备份和原子保存 |
| `ImageUtil.cpp/.h` | 图片验证、解码、缩放、预览、透明命中和图标导出 |
| `Json.cpp/.h` | JSON 解析与序列化 |
| `Util.cpp/.h` | 路径、文件、哈希、对象 ID 和内部文件复制工具 |
| `WinUtil.cpp/.h` | Windows 对话框、壁纸读取和 Explorer 桌面窗口查找 |
| `Musuka.rc` | 将 `assets/Musuka.ico` 嵌入可执行文件 |

## 运行流程

```text
wWinMain
  -> QApplication / COM / GDI+ 初始化
  -> App::Initialize
     -> ConfigStore::Load
  -> QtSettingsWindow 构造并挂接到 App
  -> App::ShowSettings
  -> QApplication::exec
```

用户点击设置页中的“运行”后，`App` 隐藏 Qt 设置窗口并创建 `DesktopWindow`。从桌面图标层返回设置时，`DesktopWindow` 被隐藏，Qt 设置页重新显示。退出时配置会保存，并恢复兼容模式下暂时隐藏的 Explorer 桌面图标。

Qt 是唯一的应用 UI 主循环。Win32 拟桌面窗口产生的消息由 Qt 的 Windows 平台事件分发器共同处理，不再维护独立的原生 Settings 窗口或第二套消息循环。

## 数据模型

`Models.h` 中的主要类型：

- `AppConfig`：桌面路径、对象列表、桌面模式、背景来源和纯色背景。
- `DesktopObject`：对象 ID、名称、路径、类型、位置、尺寸、候选图片和当前选择。
- `ImageCandidate`：显示名、原始路径、内部路径、是否为原始图标和图层优先级。

桌面对象类型包括快捷方式、文件、文件夹、“此电脑”和“回收站”。对象稳定键由对象类型与路径或 Shell ID 组成，再用于生成安全、确定性的对象 ID。

原始图标默认显示尺寸为 32 像素，替换图片默认显示尺寸为 96 像素，可调整范围为 32 到 512 像素。切换候选图片时会自动应用对应类型的推荐尺寸。

## 数据与图片存储

程序以可执行文件所在目录为应用根目录：

```text
default_image\
  Musuka.png
  *.png / *.jpg / *.jpeg / *.bmp

data\
  config.json
  icons\
    <desktop-object-id>\
      original_icon.png
      import_*.png
```

### `default_image`

`default_image` 是独立、共享的内置候选图片目录。设置页会单独枚举该目录，并向所有桌面对象提供同一份候选图。

默认图片不会：

- 复制到每个 `data/icons/<desktop-object-id>/`。
- 批量写入每个对象的 `candidates` 数组。
- 因桌面对象数量增加而重复占用磁盘和配置空间。

对象选择默认图片时，`config.json` 仅通过 `selected_image_internal_path` 保存当前默认图的相对引用。

`default_image/Musuka.png` 是应用图标源图；`assets/Musuka.ico` 由它生成，并通过 Windows 资源嵌入 `musuka.exe`。

### `data`

`data/config.json` 保存对象、位置、尺寸、模式和当前选择。对象专属目录只保存原始图标和用户导入图片。导入图片会复制到内部目录，因此删除外部原图后仍可继续使用。

配置保存使用临时文件加原子替换，并通过命名 Mutex 协调保存与临时文件清理。损坏配置会备份为 `config.corrupt_<timestamp>.json` 后重新生成。

## 桌面扫描与图标初始化

`DesktopScanner` 扫描用户选择的桌面路径，并补充当前用户桌面、公共桌面、“此电脑”和“回收站”。扫描对象包括：

- `.lnk` 快捷方式
- 普通文件
- 普通文件夹
- Windows Shell 对象

扫描后会生成稳定对象 ID，并在安全的对象目录中保存 `original_icon.png`。重新扫描时尽量保留已有对象的位置、尺寸、启用状态和当前图片选择。

## 设置界面

`QtSettingsWindow` 是唯一设置界面，分为三步：

1. 选择桌面扫描路径。
2. 配置桌面对象、候选图片、共享默认图片和显示尺寸。
3. 选择静态壁纸拟桌面模式，或从“兼容模式”中选择桌面静态壁纸兼容模式、Wallpaper Engine 动态壁纸兼容模式。

候选图片区分为：

- 对象候选图：原始图标和用户导入图。
- `default_image`：所有对象共享的内置候选图。

静态壁纸来源和纯色背景控件仅在选择静态壁纸拟桌面模式时显示。

Settings 底部导航栏提供 `中文`、`English`、`日本語` 语言选择。切换后立即重建并刷新当前 Settings 页面，同时保留当前页面、对象选择和配置状态。语言写入 `data/config.json` 的 `settings_language`，支持值：

- `zh_CN`
- `en`
- `ja`

当前本地化范围包括三步 Settings 页面、模式名称、导航与操作按钮、列表状态、预览状态、文件导入对话框标题以及 Settings 阶段显示的常见错误和警告消息。

## 桌面图标层与渲染

`DesktopWindow` 使用 GDI+ 绘制背景、替换图片、原始图标标签、选择框和选中状态。主要交互包括：

- 单击选择、框选和多选。
- 拖动并保存对象位置。
- 滚轮缩放选中对象。
- 双击打开对象。
- 右键打开、打开所在位置、管理员运行、返回设置和退出。
- 基于 PNG alpha 的透明区域命中测试。

原始图标优先采用左侧紧凑列布局；替换图片使用更大的默认尺寸和视觉留白。未保存位置的对象会自动排列，已有位置不会被自动布局覆盖。

### 静态壁纸拟桌面模式

Musuka 创建覆盖主屏幕的拟桌面窗口，绘制当前系统静态壁纸或配置的纯色背景。该模式不修改真实桌面文件。

### 兼容模式

桌面静态壁纸兼容模式和 Wallpaper Engine 动态壁纸兼容模式共用兼容模式集成。Musuka 查找 Explorer 的桌面宿主窗口，将带透明色键的桌面图标层附着到桌面宿主：

- 保留底层 Windows 静态壁纸或 Wallpaper Engine 动态壁纸画面。
- 绘制 Musuka 替换图标。
- 运行期间隐藏 Explorer 原桌面图标列表。
- 返回设置或正常退出时恢复 Explorer 原桌面图标。

`桌面静态壁纸兼容模式` 用于直接在 Windows 当前静态壁纸画面上运行；`Wallpaper Engine 动态壁纸兼容模式` 用于保留 Wallpaper Engine 动态壁纸播放。两种模式都不会加载或重绘静态壁纸，静态壁纸来源和纯色背景设置会隐藏并被忽略。

兼容模式需要能够发现 Explorer 桌面宿主窗口。如果程序被强制终止，可能需要重启 Windows Explorer 恢复原桌面图标。

### 模式配置值

`data/config.json` 中的 `desktop_mode` 使用：

- `static_wallpaper_virtual_desktop`
- `desktop_static_wallpaper_compatibility`
- `wallpaper_engine_compatibility`

旧配置值 `wallpaper` 和 `wallpaper_engine` 会分别迁移为静态壁纸拟桌面模式和 Wallpaper Engine 动态壁纸兼容模式。

## 图片处理与安全约束

图片导入支持 PNG、JPG、JPEG 和 BMP。导入前会检查扩展名、文件大小、图片头、像素尺寸和实际可解码性。

当前主要限制：

- 原始图片文件最大 10 MB。
- 解码后最大 16 MP。
- 单边最大 8192 像素。
- 未知图片格式直接拒绝。

对象目录写入前会验证路径位于 `data/icons` 内，并拒绝重解析点目录，降低配置路径逃逸和 junction/symlink 风险。完整分析见[安全审计](SECURITY_AUDIT.md)。

## 构建

### 前置条件

- Windows 10/11
- CMake 3.20+
- Visual Studio Build Tools 2022
- `Desktop development with C++`
- Qt 6.8.x MSVC 2022 64-bit

Qt 可通过 [Qt Online Installer](https://www.qt.io/download-qt-installer) 安装。至少选择 `Qt 6.x MSVC 2022 64-bit`；Qt SVG 可用于补充 SVG 图片插件。

### 构建命令

```bat
set QT_PREFIX_PATH=E:\Qt\6.8.3\msvc2022_64
build.bat
```

`build.bat` 固定生成 `build-nmake`，并执行：

1. 检查 CMake、Qt 和 `windeployqt`。
2. 查找并加载 Visual Studio x64 构建环境。
3. 使用 MSVC、NMake、Release 配置 CMake。
4. 构建 `musuka.exe` 和 `wallpaper_engine_smoke.exe`。
5. 复制 `default_image`。
6. 使用 `windeployqt` 部署 Qt DLL 与插件。
7. 部署 MSVC x64 CRT DLL。

直接运行：

```bat
build-nmake\musuka.exe
```

项目当前只接受经验证的 MSVC + NMake 构建。CMake 会拒绝其他编译器和生成器。

## 打包

```bat
package.bat
```

`package.bat` 会先清理旧 `release`，然后重新构建并生成可直接运行的动态 Qt 发布包：

```text
release\
  musuka.exe
  Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll
  msvcp140*.dll / vcruntime140*.dll
  platforms\
    qwindows.dll
  imageformats\
  default_image\
  data\
```

Qt 使用动态链接，因此发布目录必须携带 Qt DLL 和平台插件。打包脚本会检查关键运行库；如果旧 release 正被运行中的 `musuka.exe` 占用或关键依赖缺失，打包会失败，不会继续生成混杂发布包。

## 测试

构建后运行：

```bat
ctest --test-dir build-nmake --output-on-failure
```

`wallpaper_engine_smoke` 当前覆盖：

- Explorer 桌面宿主与图标列表发现。
- 桌面静态壁纸兼容模式和 Wallpaper Engine 动态壁纸兼容模式的桌面窗口附着与透明色键。
- 替换图标绘制。
- Explorer 原桌面图标隐藏与恢复。
- Qt 模式名称与兼容模式分组。
- Settings 中英日即时切换、文案和语言配置值。
- 静态壁纸选项显示与隐藏。
- 三种模式配置值序列化及旧配置值迁移。
- 纯色预览尺寸和颜色。

建议发布前同时执行：

```bat
build.bat
ctest --test-dir build-nmake --output-on-failure
package.bat
```

并从 `release` 直接启动 `musuka.exe`，确认 Qt 设置窗口能够显示和正常退出。

## 开发约束

- 不要恢复原生 Win32 Settings 界面或非 Qt 构建分支。
- 不要把 `default_image` 复制到每个对象目录或每个对象配置中。
- 不要绕过图片验证、对象 ID 验证和内部路径检查。
- 修改兼容模式集成后必须验证 Explorer 图标能够恢复。
- 修改设置界面后同步更新 Qt 冒烟测试。
- 每次可见功能、构建或兼容性更新都应在 [`docs/updates`](updates/README.md) 新增记录。
