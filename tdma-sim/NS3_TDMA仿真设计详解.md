# TDMA 控制链路仿真 — 新手完整指南

> 每条解释都标注了**对应的代码文件 + 行号**，方便你打开代码对照看。

---

## 零、先搞懂：这套仿真到底在干什么

### 0.1 一句话版本

30 架无人机 + 1 个地面站，无人机每 100ms 发一个控制包给地面站。但不能同时发——无线电频道只有一条，同时发会撞车。于是把时间切成 36 个槽（每条无人机独占一槽，轮流说话），然后统计每个包从生成到被地面站收到花了多少时间，看能不能全在 50ms 内。

### 0.2 你跑出来的数字是什么意思

```
30 机 × 5 秒 → 发了 1500 个上行包
时延均值：14.2ms   ← 平均一个包 14ms 就到地面站了
时延 P99：29ms     ← 99% 的包在 29ms 内到达
达标率：100%       ← 全部在 50ms 内
```

### 0.3 代码文件有哪些，各负责什么

| 文件 | 一句话职责 | 像什么 |
|---|---|---|
| `tdma-main.cc` | 搭场景：建多少无人机、什么参数、怎么统计 | 导演 |
| `tdma-control-mac.h` | 定义 TDMA MAC 层的对外接口 | 合同/说明书 |
| `tdma-control-mac.cc` | 实现 TDMA MAC 层的所有逻辑 | 演员 |
| `gfsk-error-model.h/.cc` | GFSK 调制的误码模型 | 道具 |

---

## 一、帧结构：时间怎么切

### 1.1 参数定义

> **文件**：`tdma-control-mac.h` 第 37-46 行  
> **文档**：§5.3.1 表 5-2

```cpp
struct TdmaFrameParams {
    uint32_t framePeriodUs   = 30000;  // 一帧 30ms = 30000μs
    uint32_t numSlots        = 36;     // 一帧切 36 块
    uint32_t uplinkSlots     = 30;     // 前 30 块给无人机用（上行）
    uint32_t downlinkSlots   = 4;      // 接下来 4 块给基站用（下行）
    uint32_t guardSlots      = 2;      // 最后 2 块留空（保护）
    uint32_t guardIntervalUs = 50;     // 每块之间留 50μs 缝隙
    uint32_t slotDurationUs  = 750;    // 每块 750μs
    uint32_t dataDurationUs  = 700;    // 其中 700μs 真的在传数据
    uint32_t maxPayloadBytes = 256;    // 每块最多塞 256 字节
};
```

**通俗理解**：把 30ms 的一帧想象成一条 36 格的传送带，每格 750μs。前 30 格归 30 架无人机（一人一格），接着 4 格归基站，最后 2 格空着。格与格之间留 50μs 的空隙防止"上一格的尾巴蹭到下一格的头"。

### 1.2 每架无人机分配哪个槽

> **文件**：`tdma-control-mac.cc` 第 76-79 行

```cpp
if (id >= 1 && id <= 30)
    m_myUplinkSlotId = id - 1;  // 无人机 ID=1 → 槽 0，ID=30 → 槽 29
```

无人机编号 1~30，槽编号 0~29。简单减 1 映射。

---

## 二、时隙调度：怎么知道"该我说话了"

这是整个仿真最核心的算法——**每个节点怎么知道自己什么时候该发**。

### 2.1 计算下一个时隙边界

> **文件**：`tdma-control-mac.cc` 第 152-170 行 `ScheduleNextSlotBoundary()`

```
第1步：从仿真开始到现在过了多久？ → elapsed
第2步：当前在第几帧的什么位置？   → posInFrame = elapsed % 30000μs
第3步：下一个槽边界在哪？        → (当前位置/750 + 1) × 750
第4步：还要等多久？             → 槽边界 - 当前位置
第5步：加上时钟漂移补偿（1ppm）  → 多等一丢丢防止时钟偏快
第6步：到点了，触发 OnSlotStart
```

**举例**：仿真刚开始 (t=0)
```
elapsed = 0
posInFrame = 0 % 30000 = 0
slotBoundary = (0/750 + 1) × 750 = 750μs
delay = 750 - 0 = 750μs
→ 在 t=750μs 触发 OnSlotStart
```

### 2.2 时隙到了之后干什么

> **文件**：`tdma-control-mac.cc` 第 172-192 行 `OnSlotStart()`

```
当前是第几个槽？ → GetCurrentSlot()
  ├─ 槽 0~29（上行）→ ProcessUplinkSlot()：检查"是我的槽吗？"
  │                    ├─ 是 → 从队列取包发出
  │                    └─ 否 → 跳过（别人在发，我听）
  ├─ 槽 30~33（下行）→ ProcessDownlinkSlot()：基站发指令
  └─ 槽 34~35（保护）→ 什么也不做
```

### 2.3 怎么判断"这是我的槽"

> **文件**：`tdma-control-mac.cc` 第 197-247 行 `ProcessUplinkSlot()`

```cpp
uint32_t slotId = GetCurrentSlot();    // 当前是第几槽
if (slotId != m_myUplinkSlotId)        // 不是我的槽？
    return;                            // → 跳过
// 是我的槽 → 从优先级队列取包 → 发出去
```

核心逻辑就两行。30 个无人机同时跑这段代码，但只有当前槽的"主人"的 `m_myUplinkSlotId` 和 `slotId` 对得上，其他 29 个直接 return——这就是 TDMA "互不干扰"的本质。

---

## 三、发包流程：一个包是怎么发出去的

### 3.1 包从哪来

> **文件**：`tdma-main.cc` 第 120-158 行 `GenerateUplinkTraffic()`

每架无人机每隔 100ms 调用一次这个函数（10Hz，对应文档 §5.3.1 的发送频率），随机生成一种指令：

| 概率 | 类型 | 包大小 | 对应文档 |
|---|---|---|---|
| 60% | 遥测数据（位置/速度/电量） | 60B | §5.4.2 紧凑编码 |
| 25% | 感知数据（障碍物检测结果） | 200B | §5.4.2 增量帧 |
| 10% | 紧急/安全指令 | 20B | §5.3.3 P0~P1 |
| 5% | 心跳消息 | 20B | §5.1 第五类业务 |

生成的包调用 `mac->EnqueueCommand()` 塞进优先级队列。

### 3.2 包怎么排队（五级优先级）

> **文件**：`tdma-control-mac.cc` 第 336-365 行 `EnqueueCommand()`

```
EnqueueCommand(类型, 优先级, 载荷, 目标)
  │
  ├─ 是 P0 紧急指令？
  │   └→ InsertEmergency()：插到 P0 队列的最前面（文件第 367-379 行）
  │       （创建一个临时队列，紧急包放第一，原 P0 队列的包跟在后面）
  │
  └─ 是 P1~P4？
      └→ m_priorityQueues[优先级].push()：正常排队（加在队尾）
```

每一级优先级有自己独立的队列（P0 队列、P1 队列……P4 队列）。发的时候从 P0 开始找，哪个队列非空就取那个队列的队头。

**为什么是 5 个独立队列而不是一个大排队？** 因为如果 P2 积了几十个包，新来的 P0 紧急指令要等到它们全发完才能轮到自己——那可能等好几帧，错过避障窗口。独立队列保证只要 P0 队列里有包，下一槽立刻发 P0。

### 3.3 包怎么组装

> **文件**：`tdma-control-mac.cc` 第 226-266 行（`ProcessUplinkSlot` 的组装部分）

每个发出的包结构如下：

```
[时间戳 8B] [序列号2B+优先级/类型1B+目标ID 1B] [实际载荷 变量] [CRC-16 2B]
```

| 字段 | 大小 | 干什么用 |
|---|---|---|
| 时间戳 | 8 字节 | 记录"这个包是几点几分生成的"，收端用来算延迟 |
| 序列号 | 2 字节 | 区分不同的包（第 1 个包 seq=1，第 2 个 seq=2……） |
| 优先级/类型 | 1 字节 | 高 4bit 存优先级 P0~P4，低 4bit 存指令类型 |
| 目标 ID | 1 字节 | 发给谁（0=地面站，1~30=某架无人机） |
| 载荷 | 10~200 字节 | 实际的控制指令内容 |
| CRC-16 | 2 字节 | 校验码——收端用这个检查包有没有在传输中损坏 |

### 3.4 CRC-16 怎么算的

> **文件**：`tdma-control-mac.cc` 第 466-483 行 `ComputeCrc16()`

用的多项式 `x^16 + x^12 + x^5 + 1`（国际标准 CRC-16 CCITT）。操作流程：

```
初始值 = 0xFFFF
对每个字节：
  crc 异或 当前字节
  反复 8 次：如果最高位是 1 → 左移一位并异或 0x1021；否则只左移一位
最终 crc 就是校验值
```

发包时把 crc 的高字节和低字节附加在包末尾。收包时（`ProcessBeacon` 第 427-435 行）重新算一遍 crc，和包尾的 2 字节比较——对不上就说明传输中出了错，丢弃。

---

## 四、收包流程：地面站怎么知道延迟多少

### 4.1 所有节点共用一个收包回调

> **文件**：`tdma-control-mac.cc` 第 104 行

```cpp
m_socket->SetRecvCallback(MakeCallback(&TdmaControlMac::ProcessBeacon, this));
```

每个 TDMA MAC 在启动时（`Start()` 第 96-107 行）创建 UDP socket，注册一个回调：**任何时候有包到达，自动调用 `ProcessBeacon`**。

### 4.2 收包后做什么

> **文件**：`tdma-control-mac.cc` 第 418-460 行 `ProcessBeacon()`

```
收到包 →
  第一步：CRC-16 校验（≥14B 的包才有 CRC 字段）
    ├─ 校验失败 → 丢包，计数 +1，跳过
    └─ 校验通过 → 继续
  
  第二步：判断是不是信标（≥12B 可能是信标）
    ├─ 是信标 → 解析帧计数器+时隙计数器 → 校准时钟
    └─ 不是信标 → 当前不做额外处理（统计在发送端完成）
```

### 4.3 时延是怎么算出来的

发包时（`ProcessUplinkSlot` 第 231 行）在包的最前面刻了生成时间戳 `nowUs`。收包时（`ProcessBeacon` 第 425 行）读这个时间戳，用"当前时间 - 生成时间"得到端到端延迟。

但是**地面站的统计不是在这里做的**——地面站的延迟统计是通过 trace 回调链路完成的。地面站收包后，TdmaApp 内部触发 `m_rxTrace`，这会调用你在 `main` 里注册的 `RxDelayCallback`（`tdma-main.cc` 第 107 行），把延迟值存入全局数组 `g_stats.rxDelays`。

---

## 五、下行指令：基站怎么给无人机发指令

### 5.1 周期性生成

> **文件**：`tdma-main.cc` 第 319-337 行

每隔 30ms（一帧的时间），基站生成 4 条下行指令：

```cpp
std::function<void()> genDl = [bsMac, nUavs, simTime, &genDl]() {
    for (uint32_t i = 0; i < 4; i++) {  // 4 条/帧
        // 按概率随机选择优先级和指令类型
        // 5% P0 紧急，20% P1 安全，70% P2 调度，5% P3 空域
        bsMac->EnqueueCommand(cmd, pri, payload, targetId);
    }
    // 过 30ms 再调一次自己
    Simulator::Schedule(MilliSeconds(30), genDl);
};
```

这里面 `std::function` 是一个能"自己调自己"的函数包装器——因为 lambda 捕获了外部的 `bsMac` 等多个变量，普通函数指针做不到这一点。

### 5.2 基站在哪个槽发

基站在槽 30~33（4 个下行时隙）发送。`OnSlotStart()` 第 184-186 行判断：

```cpp
else if (slotId >= 30 && slotId < 34) {
    if (m_isBaseStation)         // 只有基站处理下行时隙
        ProcessDownlinkSlot();   // 从自己的优先级队列取包发
}
```

无人机收到下行指令后，同样通过 `ProcessBeacon` 处理。因为下行包也包含时间戳，可以用来测量下行延迟。

---

## 六、时钟同步：怎么保证所有节点"看同一块表"

### 6.1 基站发信标

> **文件**：`tdma-control-mac.cc` 第 390-412 行 `SendBeacon()`

每 300ms（10 帧），基站广播一个 12 字节的信标：

```
[帧计数器 4B] [时隙计数器 4B] [基站ID 4B]
```

发完后，通过 `Simulator::Schedule(m_beaconInterval, &SendBeacon)` 自己预约下一次（第 411 行）。

### 6.2 无人机收信标对时

> **文件**：`tdma-control-mac.cc` 第 443-457 行

```cpp
if (srcId == 0) {  // 来自基站的信标
    m_simStartTime = Simulator::Now() 
        - MicroSeconds(frameCount × 帧周期 + slotCount × 每槽);
    // 相当于把"仿真起始时间"校准到基站的时间基准上
}
```

### 6.3 时钟漂移补偿

> **文件**：`tdma-control-mac.cc` 第 162-163 行

```cpp
double driftCorrection = m_driftPpm * 1e-6 * delayToBoundary;
delayToBoundary += driftCorrection;
```

1ppm = 每秒钟偏 1μs。如果等待 40ms，漂移 = 1×10⁻⁶ × 40000 = 0.04μs——几乎忽略不计。文档 §5.3.2 给的级联精度 ±1μs，1ppm 是保守估计。

---

## 七、半动态时隙分配

> **文件**：`tdma-control-mac.cc` 第 489-537 行 `UpdateSlotAllocation()`

```
槽 0~23：固定分配（24 架初始机）
槽 24~29：预留池（6 个空位，新来的无人机从这里分槽）
槽 30~33：基站独占
槽 34~35：保护
```

当前所有无人机在仿真一开始就注册完了（没有"半路加入"的场景），所以预留池逻辑只是骨架。当 6 个预留槽全被占时，会输出一条告警：

```cpp
NS_LOG_WARN("Reservation pool exhausted...");
```

这对应文档 §5.3.1 的"预留池耗尽时触发帧结构重分配"——骨架已经搭好，后续加新机入网逻辑时直接在这里扩展。

---

## 八、GFSK 物理层模型

> **文件**：`gfsk-error-model.h/.cc`  
> **文档**：§5.3.1 GFSK 调制参数

### 8.1 这个模型干什么

真实无线通信中，信号经过空间衰减、多径反射后，接收端解调出来的比特可能有错（0 变成 1）。GFSK 模型负责按当前信噪比（SNR）计算"每个 bit 出错的概率"（BER），然后随机翻转出错的 bit。

### 8.2 核心公式

> **文件**：`gfsk-error-model.cc` 第 87-112 行 `ComputeBer()`

```
非相干 GFSK 解调（文档未指定方式，按工程常用选非相干）：
  BER = 0.5 × exp(-α × Eb/N0 / 2)
  
  其中：
    α = 0.8（高斯滤波器引入的 ISI 惩罚因子）
    Eb/N0 ≈ SNR（因为带宽 ≈ 符号速率 = 200kHz）
```

### 8.3 当前状态

模型类编译通过但未激活——因为 CSMA 设备不支持挂载 ErrorModel。当后续换成 Wifi 信道后，一行代码激活：

```cpp
wifiDev->SetReceiveErrorModel(gfskErr);
```

---

## 九、统计系统：怎么从仿真里取数据

### 9.1 三种统计回调

> **文件**：`tdma-main.cc` 第 80-115 行

| 回调名 | 谁触发 | 什么数据 |
|---|---|---|
| `UlTxTrace` | UAV 每发一个上行包 | 延迟(ms)、无人机 ID、槽号 |
| `DlTxTrace` | 基站每发一个下行包 | 延迟(ms)、目标 ID、优先级 |
| `RxTrace` | 收到包时 | 延迟(ms)、序列号、优先级 |

回调通过 NS3 的 `TracedCallback` 机制连接——MAC 层内部触发，主程序被动接收。这样 MAC 层不需要知道"谁在监听"，主程序不需要知道"MAC 什么时候发包"。

### 9.2 注册回调

> **文件**：`tdma-main.cc` 第 325 行（基站）、第 349 行（无人机）

```cpp
// 基站：注册下行统计
bsMac->m_dlTxTrace.ConnectWithoutContext(MakeCallback(&DlTxTrace));

// 每架无人机：注册上行统计
mac->m_ulTxTrace.ConnectWithoutContext(MakeCallback(&UlTxTrace));
```

### 9.3 结果输出

> **文件**：`tdma-main.cc` 第 362-415 行

仿真跑完后，`main` 函数从全局数组 `g_stats` 中取出所有延迟数据，排序算出均值、P50、P99、最大值、达标率。同时写入 CSV 文件，格式为：

```
Metric,UL,DL,ALL
UL,Mean=14.2,P50=14,P99=29,Pass50ms=100%
```

---

## 十、完整数据流：一个包的一生（从头到尾追踪）

以 UAV1（槽 0）为例，时间线如下：

```
t=0ms      GenerateUplinkTraffic() [tdma-main.cc:120]
              → 随机生成一条指令（60%概率是遥测数据）
              → mac->EnqueueCommand(cmd, P2, payload, 0) [tdma-control-mac.cc:336]
                → 进入 m_priorityQueues[2]（P2 队列）

t=0ms      Start() [tdma-control-mac.cc:96]
              → ScheduleNextSlotBoundary() [tdma-control-mac.cc:152]
                → 下一个槽边界在 750μs
                → Simulator::Schedule(750μs, OnSlotStart)

t=750μs    OnSlotStart() [tdma-control-mac.cc:172]
              → GetCurrentSlot() = 0 [tdma-control-mac.cc:464]
              → slot 0 = m_myUplinkSlotId → 是我的槽！
              → ProcessUplinkSlot() [tdma-control-mac.cc:197]
                → 遍历 m_priorityQueues[0]→[4]
                → P2 队列非空 → 取队首
                → 组装包头：时间戳 + seq + pri/type + targetID
                → ComputeCrc16() [tdma-control-mac.cc:468]
                → 附加 2 字节 CRC
                → 限制包长 ≤258 字节（256+CRC）
                → m_socket->SendTo(基站IP, port 9) [tdma-control-mac.cc:265]
                → m_ulTxTrace(排队延迟) [tdma-control-mac.cc:268]
                   → UlTxTrace() [tdma-main.cc:80] → g_stats.ulDelays ← 延迟

t=750μs+ε 包通过 CSMA 信道到达基站
              → 基站 socket 收包回调 → ProcessBeacon() [tdma-control-mac.cc:418]
                → 第一步：VerifyCrc16() [tdma-control-mac.cc:489]
                → 第二步：解析包头，提取时间戳
                → 如果包是发给自己的 → m_rxTrace(延迟)
                  → RxTrace() [tdma-main.cc:107] → g_stats.rxDelays ← 延迟
```

---

## 十一、怎么改参数、怎么看效果

### 11.1 改无人机数量

在 `tdma-main.cc` 第 248 行修改默认值，或者运行时传参：

```bash
./build/scratch/tdma-ctrl/ns3.43-tdma-main-default --nUavs=10
```

### 11.2 改帧周期

在 `tdma-control-mac.h` 第 38 行修改 `framePeriodUs`：

```cpp
uint32_t framePeriodUs = 20000;  // 改成 20ms
```

重新编译即可。

### 11.3 看某个特定无人机的延迟

在 `tdma-main.cc` 第 82 行，`g_stats.ulSrcs` 记录了每个包的来源无人机 ID。仿真结束后遍历这个数组，筛选出某个 ID 的延迟即可。

### 11.4 加更多统计

在 `tdma-main.cc` 的 `GlobalStats` 结构体（第 55 行）里加新字段，在对应的回调函数里填充数据。

---

## 附录：所有函数速查表

| 函数名 | 文件 | 行号 | 一句话职责 |
|---|---|---|---|
| `TdmaFrameParams` | `.h` | 37 | 定义帧结构参数（30ms/36槽/750μs） |
| `PriorityLevel` | `.h` | 54 | 五级优先级枚举 P0~P4 |
| `Start()` | `.cc` | 96 | 启动 MAC：创建 socket、分配时隙、调度第一个槽 |
| `ScheduleNextSlotBoundary()` | `.cc` | 152 | 计算下一个时隙边界的时间，用 Simulator::Schedule 预约 |
| `OnSlotStart()` | `.cc` | 172 | 时隙边界到了：判断是 UL/DL/保护，调用对应处理 |
| `ProcessUplinkSlot()` | `.cc` | 197 | 上行处理：检查是"我的槽"吗→是就发包 |
| `ProcessDownlinkSlot()` | `.cc` | 282 | 下行处理：基站发调度指令 |
| `EnqueueCommand()` | `.cc` | 336 | 收到上层指令→按优先级塞进对应队列 |
| `InsertEmergency()` | `.cc` | 367 | P0 指令插入队头（不排队） |
| `SendBeacon()` | `.cc` | 390 | 基站每 300ms 发同步信标 |
| `ProcessBeacon()` | `.cc` | 418 | 收包回调：CRC 校验→信标解析/数据统计 |
| `GetCurrentSlot()` | `.cc` | 464 | 根据仿真时间算当前是第几槽 |
| `ComputeCrc16()` | `.cc` | 468 | CRC-16 CCITT 计算 |
| `VerifyCrc16()` | `.cc` | 485 | CRC-16 验证 |
| `UpdateSlotAllocation()` | `.cc` | 489 | 半动态时隙分配（预留池管理） |
| `GenerateUplinkTraffic()` | `main.cc` | 120 | 每 100ms 生成一个上行包 |
| `genDl` lambda | `main.cc` | 319 | 每 30ms 生成 4 个下行包 |
| `UlTxTrace()` | `main.cc` | 80 | 上行包发出后的统计回调 |
| `DlTxTrace()` | `main.cc` | 87 | 下行包发出后的统计回调 |
| `WriteResults()` | `main.cc` | 162 | 把统计结果写入 CSV 文件 |
