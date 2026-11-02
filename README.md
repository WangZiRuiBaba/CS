# CS2 外部辅助（ESP + 软锁）

独立进程读取 `cs2.exe` 的 `client.dll`，在透明置顶 Overlay 上绘制敌人信息，并可选内存软锁。

## 功能与快捷键

> 默认：**方框开**、**骨骼关**、**内存锁关**。切换方框 / 骨骼 / 内存锁时，屏幕下方约 3 秒中文提示。  
> 改键：编辑 [`src/aim/soft_aim.hpp`](src/aim/soft_aim.hpp) 中 `aim_key_vk`、`aim_toggle_vk` 后重新编译（`runtime.ini` / CS2Tune 仅热调锁头参数，不含改键）。

### 透视

| 功能 | 快捷键 | 操作 |
|------|--------|------|
| 方框 ESP（含左侧血量） | `Insert` | 按一下 **开/关** |
| 骨骼 ESP（头圆 + 躯干/腿线，无手臂） | `Page Up` | 按一下 **开/关** |

- 敌人筛选：存活、非本地、不同队伍
- 方框与骨骼**互不影响**，可只开骨骼、关方框

### 软锁（内存写视角）

| 功能 | 快捷键 | 操作 |
|------|--------|------|
| 内存锁（常开） | `Home` | 按一下 **开/关** |
| 软锁（按住生效） | 鼠标 **`XBUTTON1`**（侧键 / 后退键，罗技多为左侧下侧键） | **按住**期间生效 |
| 连发判定（胸线 + 强 RCS） | **`左键`** 开火 | 按住开火时由软锁逻辑识别连发 |

- `Home` 常开 与 `XBUTTON1` 按住 **任一满足即激活**软锁，也可同时用
- 点射偏头、连发胸线 + RCS，多目标粘性锁定
- 写内存：`NtWriteVirtualMemory` / `WriteProcessMemory` → `client.dll` 视角（见 `src/aim/soft_aim.cpp`）

### 程序

| 功能 | 快捷键 | 操作 |
|------|--------|------|
| 退出 ESP | `End` | 按一下退出 |

### 其它说明

- 游戏**失焦**时停止读绘并隐藏 Overlay
- 按住软锁 / 左键开火时，刷新率会从约 60 FPS 提到约 **80 FPS**（带抖动）
- **烟闪屏蔽（Page Down）**：实现仍在 [`src/sdk/entity.cpp`](src/sdk/entity.cpp)，当前构建已关闭（[`src/esp/esp.hpp`](src/esp/esp.hpp) 中 `kUtilitySuppressFeatureEnabled = false`）
- 不含 Chams、注入 DLL 等

### 快捷键一览

| 按键 | 功能 |
|------|------|
| `Insert` | 方框透视 |
| `Page Up` | 骨骼透视 |
| `Home` | 内存锁常开 |
| `XBUTTON1` | 按住软锁 |
| `左键` | 开火（连发时改变锁头/RCS 模式） |
| `End` | 退出 |

Debug 编译带控制台，启动时会打印：`Insert` 方框 · `PageUp` 骨骼 · `Home` 内存锁 · 按住 `XBUTTON1` 软锁 · `End` 退出。

## 性能与安全说明

- 前台约 **60 FPS** 刷新；按住锁头/开火时约 **80 FPS**（带随机抖动）
- 实体列表分块缓存；**仅开启骨骼时**才多读骨骼内存；**当前无敌人时**实体全量收集约 **10 Hz**（有敌人时约 30 Hz），中间帧只重算屏幕坐标
- **降低检测面（不保证安全、不能当作 VAC 安全）**：
  - Release 无控制台；Overlay 随机窗口类名、无标题、不在任务栏
  - 默认以 **`PROCESS_VM_READ`** 附加 `cs2.exe`；仅在**锁头生效**时升级为 **`VM_WRITE`**（烟闪屏蔽已关闭）
  - 句柄刷新：游戏前台且确有读内存需求时，约 **38–52 秒随机**重开；读失败时立即重连；透视/锁全关时**不做**定时刷新
  - 读内存优先 `NtReadVirtualMemory`
- **锁头关 ≠ 无风险**：仍有跨进程读、Overlay；服务器 Trust/举报与行为统计与是否开 Home 无关
- 仍可能被 VAC / 第三方反作弊检测，**勿在主号排位使用**

锁头默认参数见 [`src/aim/soft_aim.hpp`](src/aim/soft_aim.hpp) 的 `SoftAimConfig`。运行时可热调 `%LocalAppData%\CS2_ESP\runtime.ini`（推荐 **CS2Tune**，ESP 运行中也会自动重载）。

骨骼索引与连线见 [`src/sdk/bones.hpp`](src/sdk/bones.hpp)。游戏大更新后若骨骼不显示，可对照 cs2-dumper 检查 `m_modelState` 与骨骼数组偏移。

## 图形界面（推荐）

- **CS2Panel**：导出偏移、写入头文件、编译并启动 ESP
- **CS2Tune**：锁头参数白话说明 + 热调，与构建面板分开

```powershell
e:\WorkflowGame\CS\scripts\build-panel.ps1
e:\WorkflowGame\CS\scripts\build-tune.ps1
```

运行：`build\Release\Panel\CS2Panel.exe` · `build\Release\Tune\CS2Tune.exe`

面板步骤：

1. 先启动 CS2（主菜单即可）
2. **检查环境**
3. **一键：导出+写入+编译**（或分步 1→2→3）
4. **启动透视** → 游戏内见 **[功能与快捷键](#功能与快捷键)**，`End` 退出

导出偏移默认 **管理员运行 cs2-dumper**（勾选可关）。dump 结果在 `tools/dump-output/`。

## 编译（命令行）

需要 Visual Studio 2022（含 C++ 桌面开发）与 CMake 3.16+：

```powershell
e:\WorkflowGame\CS\scripts\build.ps1
```

输出：`build\Release\CS2_ESP.exe`

### Visual Studio

| 解决方案 | 内容 |
|----------|------|
| **`CS2.sln`** | 仅 **CS2Panel**、**CS2Tune**（.NET 8） |
| **`CS2.native.sln`** | 仅 **CS2_ESP**（C++，`native\CS2_ESP.vcxproj`） |

C++ 主程序需 VS **2022** +「使用 C++ 的桌面开发」+ **v143** + Windows SDK **10.0.x**。

1. 打开 **`CS2.native.sln`**
2. 选 **Release \| x64** → 生成 **CS2_ESP** → `build\Release\CS2_ESP.exe`

命令行/零售打包仍以 [`CMakeLists.txt`](CMakeLists.txt) + `scripts\build*.ps1` 为准。**新增 `.cpp` 时需同步改 `native\CS2_ESP.vcxproj` 与 CMakeLists。**

面板/热调：打开 **`CS2.sln`** 或继续用脚本：

Debug 配置会定义 `CS2_ESP_CONSOLE`，便于查看附加与开关日志。

## 更新偏移（手动）

游戏大版本更新后偏移会失效。也可用面板自动更新；手动流程：

1. 运行 `tools\cs2-dumper\cs2-dumper.exe -o tools\dump-output -f json`（CS2 已开、建议管理员）
2. 用面板 **写入 offsets.hpp**，或对照 `tools/dump-output/` 修改 [`sdk/offsets/offsets.hpp`](sdk/offsets/offsets.hpp)
3. 重新编译

面板写入时会包含 `CSkeletonInstance::m_modelState` 等骨骼所需字段。

## 使用

1. 启动 CS2，建议先 **离线练习 / 本地 Bot** 测试
2. 以普通权限运行 `CS2_ESP.exe`（读/写内存失败时可尝试与游戏相同权限）
3. Debug 版控制台出现 `Attached` 与 `client.dll` 基址后，进局用 `Insert` / `Page Up` / `Home` / 侧键等操作（见上表）

## 窗口查找

默认按窗口标题 `Counter-Strike 2` 查找；失败时按进程 PID 找最大客户区窗口，再尝试类名 `SDL_app`。

## 对外宣传文案

面向买家的名称见 [`release/使用说明.txt`](release/使用说明.txt)、[`release/商品说明.txt`](release/商品说明.txt)（全球雷达 / 体型绘制 / 磁吸装置）。程序内提示与按键说明仍以本 README 为准。
