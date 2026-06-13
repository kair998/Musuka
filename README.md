<img width="1672" height="941" alt="v0 3 0post" src="https://github.com/user-attachments/assets/871f35fa-2b9a-4d96-956e-06573b7902ec" />

# Musuka

Musuka 是一个 Windows 桌面图标娘化替换软件。当前版本不会修改真实 Windows 桌面文件，也不会修改系统图标；它会创建一个由 musuka 管理的拟桌面窗口，在背景上绘制替换图片，并通过这些图片打开对应桌面对象。

## 构建

当前项目支持两种构建模式：

| 模式 | UI 技术 | 说明 |
|------|---------|------|
| **原生模式**（默认） | Win32 API + GDI+ + Common Controls | 无需额外依赖，`build.bat` 直接编译 |
| **Qt 模式**（可选） | Qt 6 Widgets + 原生 DesktopWindow | 需安装 Qt 6，Settings 窗口使用现代 Qt UI |

### 前置条件（两种模式通用）

- Windows 10/11
- CMake 3.20+
- Visual Studio Build Tools 2022
- 安装组件：`Desktop development with C++`

### 模式一：原生构建

直接运行：

```bat
build.bat
```

脚本会：

- 查找并加载 `vcvarsall.bat x64`
- 使用 `NMake Makefiles` 配置 CMake
- 编译到 `build-nmake\musuka.exe`
- 将 `default_image\` 复制到 `build-nmake\default_image\`

构建后可运行 Wallpaper Engine 桌面层集成烟测：

```bat
ctest --test-dir build-nmake --output-on-failure
```

如果 Build Tools 安装在自定义目录，脚本会优先识别日志中验证过的路径：

```text
D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat
```

也可以手动指定：

```bat
set VCVARSALL=D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat
build.bat
```

### 模式二：Qt 构建（现代 UI）

#### 第一步：安装 Qt 6

从 [Qt 官网](https://www.qt.io/download-qt-installer) 下载 **Qt Online Installer**，安装时选择：

- **Qt 版本**：Qt 6.8.x（推荐 6.8.3）
- **编译器**：MSVC 2022 64-bit
- **组件**：
  - `Qt 6.x MSVC 2022 64-bit`（必须）
  - `Qt 6.x Additional Libraries` → `Qt SVG`（可选，支持 SVG 图片格式）

安装完成后记下安装目录，例如：

```text
E:\Qt\6.8.3\msvc2022_64
```

> **提示**：也可以只下载预编译的 Qt 库（无需完整 IDE），在线安装器里取消勾选 "Qt Creator" 和 "Qt Design Studio" 即可。

#### 第二步：构建

**方法 A — 使用快捷脚本（推荐）：**

先设置 Qt 安装路径，再运行项目根目录的 `build-qt.bat`：

```bat
set QT_PREFIX_PATH=E:\Qt\6.8.3\msvc2022_64
build-qt.bat
```

**方法 B — 手动设置环境变量：**

```bat
set USE_QT=1
set QT_PREFIX_PATH=E:\Qt\6.8.3\msvc2022_64
build.bat
```

构建成功后输出：

```text
-- musuka: Qt 6 UI support ENABLED
Build complete: build-nmake\musuka.exe
```

#### 第三步：部署运行时 DLL

构建完成后需要部署 Qt 运行时 DLL 才能正常运行。运行打包脚本即可自动完成：

```bat
package.bat
```

该脚本会检测到 Qt 构建模式，并自动调用 `windeployqt` 将以下文件复制到 `release\` 目录。
如果找不到 `windeployqt`，打包会明确失败，避免生成无法运行的残缺发布包：

- `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`
- `platforms/qwindows.dll`
- `imageformats/`（PNG/JPEG/GIF/SVG 插件）
- `styles/`（可选样式插件）

#### 第四步：运行

```bat
build-nmake\musuka.exe
```

程序启动后会同时显示：
1. **Qt 版 Settings 窗口**（带现代样式主题的 3 步向导）
2. **原版 Win32 Desktop 窗口**（点击"运行"后进入拟桌面）

#### Qt 安装目录

Qt 构建通过环境变量 `QT_PREFIX_PATH` 指定 Qt 安装目录。这样构建脚本不会依赖开发者
个人电脑上的固定盘符或目录。

生成文件：

```text
build-nmake\musuka.exe
```

## 打包

```bat
package.bat
```

生成：

```text
release\
  musuka.exe
  default_image\
    *.png / *.jpg / *.jpeg / *.bmp
  README.md
```

运行后程序会在 exe 所在目录创建：

```text
data\
  config.json
  icons\
    <desktop-object-id>\
      original_icon.png
      import_*.png
```

`default_image\` 使用扁平文件结构，目录下的图片会作为一份共享内置候选图直接被所有桌面对象引用，不会复制到每个 `data\icons\<desktop-object-id>\` 目录中，也不会展开写入每个对象的 `candidates` 配置。

用户导入的图片会复制到 `data\icons\...`，因此删除原始导入图片后，musuka 仍会使用内部备份图片。不要在后续开发中把 `default_image` 批量复制进每个对象目录，也不要把默认图批量塞进每个对象的候选数组；对象目录只保存该对象专属的原始图标和用户导入图，默认图只通过当前选中的 `selected_image_internal_path` 记录引用。

settings 的替换图片区域分为上下两块：上方显示当前对象自己的原始图标和用户导入图，下方显示共享的 `default_image` 默认图区。选择默认图时，`config.json` 只记录被选中的默认图路径，不会把所有默认图重复写入每个桌面对象。

`default_image\Musuka.png` 是 musuka 应用图标的源图；仓库中的 `assets\Musuka.ico` 由该 PNG 生成，并通过 Windows 资源文件嵌入 `musuka.exe`。settings 和 desktop 窗口会在窗口类与窗口实例上设置该图标，用于窗口左上角和任务栏显示。

## 如何运行

构建后运行：

```bat
build-nmake\musuka.exe
```

或运行打包后的：

```bat
release\musuka.exe
```

使用流程：

1. 启动 `musuka.exe`。
2. 在 `musuka settings` 第一页选择当前系统桌面文件夹。
3. 点击“下一步”扫描桌面快捷方式、普通文件、普通文件夹，并加入“此电脑”“回收站”两个 Shell 对象。
4. 在配置页选择对象、导入图片、选择候选图片并点击“替换”。
5. 用“带入 / 忽略”控制该对象是否进入 `musuka desktop`。
6. 在模式页选择 `静态壁纸模式`，或在 Wallpaper Engine 正在运行时选择 `Wallpaper Engine 动态壁纸兼容模式`。
7. 点击“运行”进入拟桌面。

## 当前支持

- 扫描桌面 `.lnk` 快捷方式。
- 扫描桌面普通文件。
- 扫描桌面普通文件夹。
- 支持“此电脑”和“回收站”。
- 支持 PNG/JPG/JPEG/BMP 图片导入。
- 读取 exe 所在目录下扁平结构的 `default_image\` 作为共享内置候选图片来源。
- 每个桌面对象默认导入原始图标候选项。
- 可在 settings 中为每个桌面对象调整显示尺寸，也可在 desktop 中选中后用滚轮缩放。
- 拟桌面显示替换图片；使用原始图标候选项时显示文件名称。
- 使用原始图标候选项的对象会优先按左侧紧凑列布局排列，替换图片保留更大的视觉留白。
- 单击选中、左键框选、拖动保存位置、双击打开。
- 右键菜单：打开、打开所在位置、以管理员身份运行、返回 settings、退出 musuka。
- PNG alpha 点击命中：透明区域不会触发点击、拖动、双击。
- Wallpaper Engine 动态壁纸兼容模式：保留动态壁纸画面，在桌面图标层显示 musuka 替换图标，并暂时隐藏 Windows 原桌面图标。
- 配置持久化到 `data\config.json`。

## 当前未实现

- 不支持 Live2D、视频背景、插件系统、网络下载、账号系统、云同步。

## 注意事项

- `静态壁纸模式` 通过全屏拟桌面窗口接管视觉显示，不修改 Windows 原桌面图标状态。只有选择该模式时，settings 才会显示静态壁纸来源与纯色背景设置。
- `Wallpaper Engine 动态壁纸兼容模式` 会在运行期间隐藏 Explorer 的原桌面图标列表；正常退出或返回 settings 时会恢复。若 musuka 被强制终止，可重启 Windows Explorer 恢复原桌面图标。
- 当前版本不会删除、移动或修改用户真实桌面文件。
- 替换图片在 desktop 阶段会按对象配置尺寸等比绘制：原生图标默认 32 像素，替换图片默认 96 像素，可在 32 到 512 像素之间调整；推荐使用带透明 alpha 通道的 PNG。
- 如果 `default_image` 目录不存在，程序仍可运行，但会提示默认图片为空。
- 如果 `data\config.json` 损坏，程序会自动备份为 `config.corrupt_时间戳.json` 并重新生成配置。

## 简单测试方案

1. 启动 musuka。
2. 选择系统桌面路径。
3. 确认能扫描到桌面快捷方式、普通文件和普通文件夹。
4. 选择一个对象，导入单张 PNG 图片。
5. 导入一个图片文件夹。
6. 选择候选图片并点击“替换”。
7. 将另一个对象设置为“忽略”，确认列表显示忽略状态。
8. 进入 `静态壁纸模式`。
9. 选择“使用当前系统静态壁纸”，点击运行，确认背景绘制为系统静态壁纸。
10. 返回 settings，选择“使用 musuka 纯色背景”，选择颜色并运行。
11. 在拟桌面拖动一个替换图片，退出后再次启动，确认位置保持。
12. 双击快捷方式、文件夹、“此电脑”、“回收站”，确认能打开。
13. 右键替换图片，测试“打开”“打开所在位置”“以管理员身份运行”“返回 settings”“退出 musuka”。
14. 关闭后再次打开，确认配置仍然存在。
15. 删除某张原始导入图片，再次进入 desktop，确认 musuka 内部备份图片仍可使用。
16. 确认 `data\icons\<desktop-object-id>\` 中没有 `default_...` 内置图副本，默认候选图只来自共享的 `default_image\`。
17. 启用 Wallpaper Engine 动态壁纸后进入 `Wallpaper Engine 动态壁纸兼容模式`，确认动态壁纸保持播放、替换图标可见、Windows 原桌面图标隐藏；返回 settings 后确认原桌面图标恢复。

## 欢迎打赏
<img width="996" height="996" alt="58b38e0ed8e17a3e3f84c9d4dc59ce2d" src="https://github.com/user-attachments/assets/a90fb9b8-dfd1-43af-be88-2a9c98d40f84" />
