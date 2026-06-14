<img width="1672" height="941" alt="v0 3 0post" src="https://github.com/user-attachments/assets/871f35fa-2b9a-4d96-956e-06573b7902ec" />

# Musuka

## 文档

- [技术文档：项目结构、构建、打包与实现细节](docs/TECHNICAL.md)
- [性能优化报告](docs/PERFORMANCE_OPTIMIZATION.md)
- [更新记录](docs/updates/README.md)
- [安全审计](docs/SECURITY_AUDIT.md)

Musuka 是一个 Windows 桌面图标娘化替换软件。当前版本不会修改真实 Windows 桌面文件，也不会修改系统图标；它会创建一个由 Musuka 管理的桌面图标层，在拟桌面背景或当前桌面壁纸上绘制替换图片，并通过这些图片打开对应桌面对象。

## 如何运行

运行打包后的：

```bat
release\musuka.exe
```

开发构建和打包方式见[技术文档](docs/TECHNICAL.md#构建)。

使用流程：

1. 启动 `musuka.exe`。
2. 在 `musuka settings` 第一页选择当前系统桌面文件夹。
3. 点击“下一步”扫描桌面快捷方式、普通文件、普通文件夹，并加入“此电脑”“回收站”两个 Shell 对象。
4. 在配置页选择对象、导入图片、选择候选图片并点击“替换”。
5. 用“带入 / 忽略”控制该对象是否进入 `musuka desktop`。
6. 在模式页选择 `静态壁纸拟桌面模式`，或从“兼容模式”中选择 `桌面静态壁纸兼容模式`、`Wallpaper Engine 动态壁纸兼容模式`。
7. 点击“运行”进入拟桌面。

## 当前支持

- 扫描桌面 `.lnk` 快捷方式、普通文件和普通文件夹。
- 支持“此电脑”和“回收站”。
- 支持 PNG/JPG/JPEG/BMP 图片导入。
- 使用共享 `default_image` 作为所有桌面对象的内置候选图片来源。
- 每个桌面对象默认提供原始图标候选项。
- 可在 settings 中调整对象显示尺寸，也可在 desktop 中选中后用滚轮缩放。
- 原始图标优先按左侧紧凑列布局排列，替换图片保留更大的视觉留白。
- 单击选中、左键框选、拖动保存位置、双击打开。
- 右键菜单：打开、打开所在位置、以管理员身份运行、返回 settings、退出 Musuka。
- PNG alpha 点击命中：透明区域不会触发点击、拖动或双击。
- Musuka Settings 支持中文、English、日本語即时切换，并记住语言选择。
- 静态壁纸拟桌面模式：由 Musuka 绘制当前系统静态壁纸或纯色背景。
- 桌面静态壁纸兼容模式：直接保留 Windows 桌面静态壁纸画面，仅覆盖显示 Musuka 图标。
- Wallpaper Engine 动态壁纸兼容模式：保留动态壁纸，显示 Musuka 替换图标，并暂时隐藏 Windows 原桌面图标。
- 配置持久化到 `data\config.json`。

## 当前未实现

- Live2D
- 视频背景
- 插件系统
- 网络下载
- 账号系统
- 云同步

## 注意事项

- `静态壁纸拟桌面模式` 通过全屏拟桌面窗口接管视觉显示，不修改 Windows 原桌面图标状态。
- `桌面静态壁纸兼容模式` 和 `Wallpaper Engine 动态壁纸兼容模式` 归入“兼容模式”，都会在运行期间隐藏 Explorer 原桌面图标列表；正常退出或返回 settings 时会恢复。
- 若 Musuka 在兼容模式下被强制终止，可重启 Windows Explorer 恢复原桌面图标。
- 当前版本不会删除、移动或修改用户真实桌面文件。
- 原始图标默认 32 像素，替换图片默认 96 像素，可在 32 到 512 像素之间调整。
- 推荐使用带透明 alpha 通道的 PNG。
- 如果 `default_image` 目录不存在，程序仍可运行，但不会提供内置候选图片。
- 如果 `data\config.json` 损坏，程序会自动备份并重新生成配置。
- Qt 使用动态链接，发布时必须保留 `release` 中随程序打包的 DLL 和插件目录。

## 欢迎打赏

<img width="996" height="996" alt="58b38e0ed8e17a3e3f84c9d4dc59ce2d" src="https://github.com/user-attachments/assets/a90fb9b8-dfd1-43af-be88-2a9c98d40f84" />

## 第三方素材声明

`default_image\` 中以 `touhou_` 开头的图片来自 Ryogo / りょうご制作的
[东方 Project SD 素材集](http://p-lux.net/material_tohosd.php)，角色原作为
Team Shanghai Alice 的东方 Project。

这些图片是非官方二次创作素材，不属于 Musuka 的源代码许可证。使用、修改或
再分发时，需要分别遵守素材作者的使用条件与
[东方 Project 二次创作指南](https://touhou-project.news/guideline/)。
详细文件清单与署名信息见
[`default_image/THIRD_PARTY_ASSETS.md`](default_image/THIRD_PARTY_ASSETS.md)。
