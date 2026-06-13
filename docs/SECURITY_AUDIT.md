# 安全审计

审计日期：2026-06-06
最后更新：2026-06-13

本报告基于静态源码审查。项目已有集成冒烟测试，但尚无专门的自动化安全测试。

## 已修复

### 高风险：配置对象 ID 可导致目录逃逸（已修复）

**防护措施：**
- `IsSafeRelativeId()` 拒绝 Windows 保留设备名、尾随/前导点号，
  仅允许 `[A-Za-z0-9_-]`
- **`IsPathInsideIconsRoot()` 通用函数**：icons 根目录用 `fs::canonical`
  验证存在且真实；候选路径用 `fs::weakly_canonical` 解析
  （支持尚不存在的路径）；路径分隔符守卫的严格前缀匹配
- `DesktopScanner::InitializeObjectImages`、`QtSettingsWindow::addCandidateFromFile`
  统一调用 `IsPathInsideIconsRoot`
- **加载时强制重新生成所有对象 ID**（不再依赖 JSON 中的原始 ID），
  使用 `MakeObjectId(name, stableKey)` 确保确定性；
  fallback 为 hash-based ID
- 对象数组上限截断至 1024

证据：
- `src/Util.cpp:393-417`, `src/Util.h:49-55`
- `src/DesktopScanner.cpp:273-284`
- `src/QtSettingsWindow.cpp:1789-1799`
- `src/ConfigStore.cpp:269-285`

### 中风险：图片解码拒绝服务（已修复）

**防护措施：**
- GDI+ 解码前检查原始文件大小上限 **10 MB**
- **解码前手动解析 PNG IHDR / JPEG SOF / BMP DIB 头获取像素尺寸**：
  - PNG/BMP 用固定偏移量读取（小缓冲区即可）
  - JPEG **一次性读取最多 256 KB 到连续缓冲区**再扫描段标记，
    消除跨块解析 bug；正确处理 FF 填充、EOI/SOS 终止、无效长度
  - **未知格式一律拒绝**，零维度拒绝
- 解码后像素上限 **16 MP**，单维度上限 **8192**

证据：
- `src/ImageUtil.cpp:56-165` (头解析), `183-191` (集成调用)

### 中风险：管理员运行信任持久化路径（已修复）

**防护措施：**
- 仅对已知可执行扩展名允许提权（`.exe`, `.bat`, `.cmd`, `.ps1` 等）
- **`.lnk` 已从白名单中移除**：快捷方式可指向桌面外任意目标，
  解析需要 COM 且无法保证可信
- **桌面路径验证使用 `SHGetKnownFolderPath(FOLDERID_Desktop)` 和
  `FOLDERID_PublicDesktop` 作为可信根目录**（不再信任 config.desktopPath），
  严格前缀匹配 + 分隔符守卫，同时覆盖用户桌面和公共桌面
- 提权前弹出确认对话框：显示完整目标路径 +
  配置名称 vs 实际文件名（不一致时同时显示）
- 右键菜单中"以管理员身份运行"项同步灰化非可执行文件和 .lnk

证据：
- `src/DesktopWindow.cpp:964-1014, 1030-1034`

### 低风险：临时配置文件多实例竞态（已修复）

**防护措施：**
- **保存和清理共享同一命名 Mutex** (`Local\musuka_config_cleanup`)：
  清理时先获取锁；**写入时全程持有同一 Mutex**
- 时间戳毫秒级 + **`CryptGenRandom` 加密随机后缀**
- 替换失败时删除临时文件

证据：
- `src/ConfigStore.cpp:223-245, 298-357`

### 中风险：JSON 数组/成员数无上限（已修复）

**防护措施：**
- 数组元素上限 **65536**，对象成员数上限 **4096**

证据：
- `src/Json.cpp:218-224, 249-254`

## 部分缓解

### 目录写入 TOCTOU 竞态（部分缓解）

**已实施的防护：**
- 操作前调用 `IsPathInsideIconsRoot()` 验证路径包含关系
- `EnsureDirectory()` 后、写入/删除前调用 `IsReparsePoint()`
  检查目录是否被替换为 junction/symlink
- 新增 `IsReparsePoint()` 函数基于 `GetFileAttributesW` + `FILE_ATTRIBUTE_REPARSE_POINT`

**剩余风险：**
检查与实际操作之间存在时间窗口。攻击者可在 reparse point 检查之后、
文件写入之前原子性地替换目录。彻底消除此竞态需要：
1. 打开目录句柄后在该句柄上执行所有子操作（`FILE_FLAG_OPEN_REPARSE_POINT`），
   或
2. 使用事务性 NTFS 操作（TxF / `CreateFile` + `TRANSACTIONAL`）。

当前方案将攻击窗口从"任意时刻"缩小到"检查后到首次文件操作前"的极短区间，
对非特权本地攻击者有实际防御效果。

证据：
- `src/Util.h:53-55`, `src/Util.cpp:413-417`
- `src/DesktopScanner.cpp:279-284`
- `src/QtSettingsWindow.cpp:1790-1799`

---

## 维护建议

- 增加针对路径逃逸、恶意图片和损坏配置的专门安全回归测试。
- 增加静态分析、模糊测试和 CI。
