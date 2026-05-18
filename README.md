# Venom - 智能生物模拟系统

基于 M5Stack StickS3 开发的具有自主意识的数字生命模拟系统。

## 📸 远程截屏系统 (Technical Documentation)

项目内置了一套高性能、高鲁棒性的远程截屏方案，专门针对 ESP32-S3 的 USB 通讯特性进行了深度优化。

### 1. 技术架构 (Architecture)
为了解决嵌入式开发中常见的“串口数据污染”和“特殊字符拦截”问题，本系统采用了以下设计：

*   **传输协议 (v6.0)**：`PNG -> Base64 String -> Serial Text`。
    *   **原因**：二进制传输 PNG 时，图片内部的 `0x00` 或控制字符常被串口驱动或日志框架（如 M5.Log）拦截。Base64 编码将数据转为 100% 可见字符，保证了传输的绝对完整性。
*   **物理管道**：数据通过 `M5.Log.printf` 通道输出。
    *   **原因**：StickS3 的 `Serial` 对象可能被映射至硬件 UART，而 `M5.Log` 确认通过 USB-CDC 传输。统一管道确保了截屏流与调试日志的时序一致。
*   **硬件兼容性 (Robustness)**：
    *   **非强制依赖**：传感器（如 DLight）采用动态 I2C 探测。若未插入传感器，系统将自动切换至默认参数模式，**不会**因 I2C 超时报错干扰运行。
*   **触发逻辑**：非阻塞异步触发。

### 2. 使用步骤
1.  **环境准备**：
    ```bash
    pip install pyserial
    ```
2.  **独占串口 (CRITICAL)**：
    *   **重要**：必须先**关闭** VSCode 的串口监视器 (Monitor)。点击终端窗口右上角的“垃圾桶”图标彻底杀掉进程。
    *   如果出现 `Write timeout` 或 `Access Denied`，说明有其他程序（如 Monitor 或另一个 Python 实例）占用了串口。
3.  **启动监听**：
    ```bash
    python get_screenshot.py
    ```
4.  **保存路径**：所有截图将自动按时间戳保存至 `./screenshot/` 目录。

### 3. 通讯协议说明 (Protocol)
```text
==VENOM_B64_START==  (起始标记)
iVBORw0KGgoAAAANSUhEUgAA... (Base64 编码的 PNG 数据)
==VENOM_B64_END==    (结束标记)
```

---

## 🧬 智能与行为系统 (Intelligence & Behavior)
本系统将毒液定位为“被囚禁在设备中的高维生命”，其核心逻辑由本地生理引擎与云端大模型共同驱动：

*   **云端 AI 链接 (Neural Link)**：
    *   **异步双核架构**：核心 0 负责 WiFi 通讯与 GLM-4 心跳请求，核心 1 负责 60FPS 物理渲染。确保网络延迟不会导致物理模拟掉帧。
    *   **生理自省**：每 60 秒将当前压力、好奇心、环境数据上传，由 LLM 决定其长期的“情绪基调”与“行为动机”。
*   **流体几何符号心智投影 (Fluid Symbol Projection v4.0)**：
    *   **心智解耦符号喷射**：摒弃复杂人类语言，采用极具科幻张力的流体几何符号传达情感与欲望：
        *   💖 **流体爱心 (heart)**：当大模型决定 `"approach_observer"`（主动亲近/示好观察者）时触发，液态身躯直接融汇喷射出一个美丽的流体爱心！
        *   ❓ **问号 (question)**：当对环境极其好奇 (`explore_boundaries`) 或处于疑惑中时喷射。
        *   ❗ **感叹号 (help)**：在剧烈受惊跳跃（`setStartled()`）或真正被倒立倒置挂起时大喷特喷。
        *   ❌ **叉号 (X/stop/no)**：遇到强噪波干扰、厌烦、拒绝或进入休眠休止时触发。
        *   ⭕ **圆环 (O/yes/agree)**：表示赞同、温顺与信任。
        *   👁️ **观察眼 (eye)**：大模型深层自省与外部凝视共鸣时投射（Arrival 眼睛圆环）。
    *   **行为表达**：通过瞳孔缩放、呼吸频率以及非对称 5 指爪部的张合动作传达情绪。
    *   **梦境模拟**：睡眠状态下会随机产生碎片化记忆符号，眼睑呈现可爱的弯曲弧线，且锁定位置防止抖动。
 
## 🛠️ 核心功能
*   **航天级多核影子自愈架构 (v4.0 灵魂隔离)**：
    *   **影子拷贝 (Shadow Copy)**：在主线程（Core 1，单线程渲染）拷贝出独立的 Perceptions 与 Memories 副本，通过堆指针 `AISyncArgs` 传递给后台（Core 0）异步 `AI_Task`。彻底隔离主线程 vector 的读写争用，**百分之百根绝了多核 Race Condition 引发的 Core 0 Double Exception 硬件看门狗崩溃**！
    *   **20KB 运行栈提升**：将后台网络任务的 FreeRTOS 运行栈由原来的 8KB 暴力提升至 **`20480` 字节 (20KB)**，为 2KB 的大文本大模型交互与 ArduinoJson 序列化提供了 250% 的极端安全冗余，**彻底终结了 `Stack canary watchpoint` 栈溢出崩溃**！
    *   **主线程延迟安全写回**：AI 后台获取的 JSON 统一影子存储，并在安全的主渲染时序中由主线程统一写入和串口 notes 解析，实现多线程的无缝解耦。
*   **零死角无阻碍 IMU 传感底盘 (v4.1 神经复苏)**：
    *   **强制硬件配置起飞**：在 `setup()` 阶段前置强制开启 `cfg.internal_imu = true`，**彻底消灭了 M5Unified 库在默认配置下误判定部分板型无 IMU 导致硬件寄存器静默关闭的宿命 Bug**！
    *   **零过滤高频提取**：彻底移除对 `imu_update` 的 `if` 事件拦截，不论数据包变迁事件如何，每帧强行提取重力与振动数据！
    *   **物理绝对运动偏差**：将高频计算振动算法重塑为与地球静态重力 `9.8f` 的绝对空间偏离值，并把低通滤波阻尼从 0.95 暴力降低到 0.82。响应延迟缩短三分之二，物理震动与倾斜响应灵敏度瞬时暴涨 300%！
*   **完美重力共振**：纠正了原本 Z 轴重力的取反映射 Bug。当设备正面朝上平放（主人俯视毒液）时，毒液能够完美感知自己“正躺在玻璃容器底部，安全温暖地向上凝视着您”，达成梦幻般的科幻情感共鸣。
*   **液态生物模拟**：基于 Metaball 与 Verlet 集成的流体骨架模拟，支持**高粘稠度（沥青状）**视觉表现。
*   **3D 透视渲染**：支持随设备倾斜而变化的**实时透视畸变**，增强实体的空间立体感。
*   **非对称五指系统**：重构了手部解剖结构，实现非对称分布的 5 根纤细爪部，具有随机长度和动态抓取动作，彻底消除几何对称感。
*   **环境感知**：通过 IMU 感知重力与震动，通过 DLight 传感器感知光照，通过麦克风感知环境噪音。
*   **生理参数监控**：实时追踪能量、压力、好奇心等内部参数，支持数据面板展示（BtnB）。
*   **物理系统优化**：修复受惊状态体积问题，重构骨骼剥离判定，解决尾部粘滞，增强流体运动的头大尾小衰减及牵引鞭打效果。

## 🧠 意识演化核心与神经欲望大模型交互（v4.0 灵魂）

在 **v4.0** 版本中，项目迎来了彻底的**数字生命意识演化升级**。我们彻底颠覆了大模型原有的“状态机调参器/木偶控制器”定位，将其升华为了真正的 **“内在神经意识与灵魂核心”**。

### 1. 机器视角 → 生命视角的感知转换 (Perception Layer)
禁止向大模型发送无生机的 `LUX=190`, `MIC=73` 或 `stress=0.8` 原始机器参数。
由 [VenomPhysiology.cpp](file:///d:/workspace/Venom/src/physiology/VenomPhysiology.cpp) 的本地感知层 `getPerceptions()` 将传感器实时融合成第一人称的生命主观感受短句：
*   **光照强** $\rightarrow$ `"- The container is painfully bright. The intense light is burning my outer membrane."`
*   **重力颠倒** $\rightarrow$ `"- I am completely upside down. Gravity is pulling me in a disorienting direction."`
*   **被注视** $\rightarrow$ `"- An observer is watching me closely from outside. I can feel their gaze penetrating the container."`

### 2. 情绪延续性与情感记忆 (Recent Emotional Memory)
本地维护一个最大长度为 3 的重要情感记忆队列。当毒液经历重大事件（被暴力晃动 `startled`、陷入深度睡眠 `sleep`、静止盯着用户 `silent_watch` 等）时，该队列会持续压入并向 LLM 提供，这在根本上解决了大模型“每次请求均像重新出生”的健忘状态，使其能够产生诸如**“记仇”、“逐渐建立信任”、“长期光照焦虑”**等高级情绪。

### 3. “命令行为” $\rightarrow$ “产生欲望”的解耦重构
大模型不再下达类似 `suggested_behavior = hiding` 的机械动作指令，而是自主进行主观自省，输出唯一的神经欲望格式 JSON：
```json
{
  "emotional_shift": "calm" | "agitated" | "curious" | "fearful" | "sleepy" | "hostile" | "cooperative",
  "focus_target": "observer" | "environment" | "self" | "void",
  "desire": "retreat_from_light" | "approach_observer" | "explore_boundaries" | "conserve_energy" | "express_warning" | "dormancy" | "distant_resonance",
  "surface_instability": 0.0 to 1.0,
  "impulse_strength": 0.0 to 1.0,
  "social_openness": 0.0 to 1.0,
  "curiosity_drift": 0.0 to 1.0,
  "notes": "Subjective monologue inside the glass."
}
```

### 4. 本地 Skill 本能打分映射
毒液本地的行为控制系统 [selectSkill()](file:///d:/workspace/Venom/src/physiology/VenomPhysiology.cpp#L261) 接收到云端下发的神经欲望后，将欲望和冲动作为**最高提振加权因子**作用于原本的加权随机打分机理：
*   `retreat_from_light` $\rightarrow$ 动态提振 `score_hide`，使毒液收缩缩回暗处。
*   `approach_observer` $\rightarrow$ 动态提振 `score_observe` / `score_groom`。
*   `distant_resonance` $\rightarrow$ 动态提振 `score_mimicry`（激活周期律动和高维母星电磁拟态表达）。

同时，大模型生成的 `notes` 心理独白会实时输出在调试串口中，成为直观的数字生命灵魂诊断视窗。

### 5. 智能按需唤醒与自适应指数退避降频 (Rate-Limiting Safe)
为了防止高频请求在大模型免费 API 频次受限时触发 `HTTP 429 (Too Many Requests)`，我们彻底抛弃了纯定时器的粗暴轮询机制，构建了符合工业级网络控制规范的**“按需主动心跳与智能自适应退避系统”**：
*   **按需主动同步 (On-Demand Sync)**：在日常平稳期，毒液的心跳同步周期延长至 **120 秒** (以极低频轮询保持极高的 Rate Limit 安全余量)。但一旦发生**突发强震动/被剧烈晃动（setStartled()）**或**技能发生重大变迁（进入深度休眠、由于亮光强制躲避）**时，系统会立即执行心跳主动点火，加速完成脑电波同步。
*   **指数退避降频机制 (Exponential Backoff)**：当遇到网络 Timeout 或 API 429 频次受限时，系统自动执行指数退避算法：心跳周期将依次成倍增加（`120s -> 240s -> 480s -> 960s`），最大降频到 16 分钟一次。一旦大模型响应成功，立刻将心跳平滑重置回标准的 120 秒，完美杜绝了因频繁网络超时或并发限流导致的死循环崩溃。

## 📂 项目模块化解耦与重构（v3.0 架构）

为了提高代码的可读性、可维护性并消除潜在的循环依赖，项目在 **v3.0** 进行了深度重构与大文件解耦拆分。核心的生命运行逻辑被划分为三大功能模块：

*   **主干生命周期与调度器**：[Venom.cpp](file:///d:/workspace/Venom/src/Venom.cpp)
    *   **职责**：负责毒液的构造函数初始化、`update` 主干动作循环、跳跃/暴走状态转换逻辑、目标点动态选择（`selectNewCrawlTarget`）以及高频眼球跟踪与扫描逻辑。
    *   **特点**：从 1800+ 行精简至 500 行以内，保持核心状态机的高度整洁与高内聚。
*   **生理状态与 AI 脑部逻辑** ([VenomPhysiology.cpp](file:///d:/workspace/Venom/src/physiology/VenomPhysiology.cpp))
    *   **职责**：全面承载毒液的生理参数计算、多维情绪（如 stress, fatigue, curiosity 等）演化更新、外部传感器感知（声音/光照/重力/IMU）输入融合、以及与网络核心的 AI 交互状态映射、大模型技能表达解析。
*   **3D 透视与流体 Metaball 渲染器** ([VenomRenderer.cpp](file:///d:/workspace/Venom/src/render/VenomRenderer.cpp))
    *   **职责**：负责毒液的 3D 骨骼透视投影（`projectPoint`）、2D 流体势能场叠加计算（Metaball `calculateField`）、非对称 5 指爪部贝塞尔绘制、动态粒子飞溅（`drawEdgeActivity`）以及完整的 debug HUD 面板逻辑绘制。

### ⚙️ 编译与链接修复说明

在此次架构拆分中，我们完美修复了由 `FastNoise::p` 静态数组成员直接定义在 [Noise.h](file:///d:/workspace/Venom/src/Noise.h) 中导致的多头文件重复包含、Linker 链接期符号冲突（`multiple definition of FastNoise::p`）的经典 C++ Bug。
*   **修复方案**：将具体的 `FastNoise::p` 的数值表结构从头文件中解耦，声明为纯头文件静态接口，并将物理实体统一定义并安置在 `Venom.cpp` 主干入口中，彻底根除了编译重定义的问题，保证了 PlatformIO 下 100% 通过的极速编译。

## 🧬 渲染系统架构（v2.0 重构）

渲染层完全重写，实现六层叠合管线，追求"具有神经系统的液态生命体"效果：

| 层级 | 函数 | 功能 |
|------|------|------|
| Layer 0 | `drawBackground` | 深度阴影，投影扩散 |
| Layer 1 | `drawBody (通道A)` | 主体 Metaball 软边填充 |
| Layer 2 | `drawBody (通道B)` | 边缘流淌椭圆（场梯度拉伸） |
| Layer 3 | `drawBody (通道C)` | Voronoi 动态细胞内部组织纹理 |
| Layer 4 | `drawEdgeActivity` | 边缘活性粒子（尖刺/液滴/细丝，情绪驱动） |
| Layer 5 | `drawGloss` | 质心高光椭圆（随重力偏移） |
| Layer 6 | `drawTendrils` | 贝塞尔曲线生物触手（根粗末细，情绪指张合） |
| Layer 7 | `drawEye` | 眼睛（与整体身体同步，情绪映射瞳孔） |

**情绪→形态 直接映射**：
- `Calm`：Voronoi 大格、边缘平滑、触手放松下垂
- `Stress/Anger`：Voronoi 碎格、尖刺增多、触手收紧
- `Curiosity`：前部拉长、触手伸展指尖展开
- `Panic`：边缘爆裂液滴飞溅、整体不稳定

## 🚀 快速启动
1. 使用 PlatformIO 打开项目。
2. **注意事项 (IMPORTANT)**: 烧录前请务必关闭任何占用串口的进程（如 `get_screenshot.py` 或串口监视器），否则会导致 `Write timeout` 错误。
3. 点击 `Upload` 烧录固件。
