<img width="1672" height="941" alt="ChatGPT Image 2026年5月27日 01_07_15" src="https://github.com/user-attachments/assets/d92a2a12-06cb-4f92-9b3f-0c776701d0eb" />

# Musuka

Musuka 是一个 Windows 桌面图标娘化替换软件。当前版本不会修改真实 Windows 桌面文件，也不会修改系统图标；它会创建一个由 musuka 管理的拟桌面窗口，在背景上绘制替换图片，并通过这些图片打开对应桌面对象。

## 构建

当前项目只保留一种已验证的构建方式：**Visual Studio Build Tools 2022 + MSVC 开发者环境 + CMake NMake generator**。

要求：

- Windows 10/11
- CMake 3.20+
- Visual Studio Build Tools 2022
- 安装组件：`Desktop development with C++`

直接运行：

```bat
build.bat
```

脚本会：

- 查找并加载 `vcvarsall.bat x64`
- 使用 `NMake Makefiles` 配置 CMake
- 编译到 `build-nmake\musuka.exe`
- 将 `default_image\` 复制到 `build-nmake\default_image\`

如果 Build Tools 安装在自定义目录，脚本会优先识别日志中验证过的路径：

```text
D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat
```

也可以手动指定：

```bat
set VCVARSALL=D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat
build.bat
```

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

`default_image\` 使用扁平文件结构，目录下的图片会作为一份共享内置候选图直接被所有桌面对象引用，不会复制到每个 `data\icons\<desktop-object-id>\` 目录中。

用户导入的图片会复制到 `data\icons\...`，因此删除原始导入图片后，musuka 仍会使用内部备份图片。不要在后续开发中把 `default_image` 批量复制进每个对象目录；对象目录只保存该对象专属的原始图标和用户导入图。

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
6. 在模式页选择 `Wallpaper 模式`，并选择当前系统静态壁纸或 musuka 纯色背景。
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
- 单击选中、左键框选、拖动保存位置、双击打开。
- 右键菜单：打开、打开所在位置、以管理员身份运行、返回 settings、退出 musuka。
- PNG alpha 点击命中：透明区域不会触发点击、拖动、双击。
- 配置持久化到 `data\config.json`。

## 当前未实现

- `Wallpaper Engine 模式` 仅作为保留选项显示。选择该模式会提示“该模式暂未实现，当前版本请使用 Wallpaper 模式”，不会执行动态壁纸兼容逻辑。
- 不支持 Live2D、视频背景、插件系统、网络下载、账号系统、云同步。

## 注意事项

- 当前版本不会隐藏 Windows 原桌面图标。`musuka desktop` 通过一个全屏拟桌面窗口接管视觉显示；退出或返回 settings 后不会影响真实桌面。
- 当前版本不会删除、移动或修改用户真实桌面文件。
- 替换图片在 desktop 阶段会按对象配置尺寸等比绘制，默认 128 像素，可在 48 到 512 像素之间调整；推荐使用带透明 alpha 通道的 PNG。
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
8. 进入 `Wallpaper 模式`。
9. 选择“使用当前系统静态壁纸”，点击运行，确认背景绘制为系统静态壁纸。
10. 返回 settings，选择“使用 musuka 纯色背景”，选择颜色并运行。
11. 在拟桌面拖动一个替换图片，退出后再次启动，确认位置保持。
12. 双击快捷方式、文件夹、“此电脑”、“回收站”，确认能打开。
13. 右键替换图片，测试“打开”“打开所在位置”“以管理员身份运行”“返回 settings”“退出 musuka”。
14. 关闭后再次打开，确认配置仍然存在。
15. 删除某张原始导入图片，再次进入 desktop，确认 musuka 内部备份图片仍可使用。
16. 确认 `data\icons\<desktop-object-id>\` 中没有 `default_...` 内置图副本，默认候选图只来自共享的 `default_image\`。

