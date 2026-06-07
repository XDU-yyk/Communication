/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/**
 * TDMA控制链路MAC层实现
 * 
 * 对应文档：第5章 确定性通信与自组网子系统设计
 *   5.3.1节 TDMA帧结构：帧周期30ms, 36时隙
 *   5.3.3节 指令优先级：P0~P4五级
 *   5.3.4节 50ms低时延保障：四段预算分解
 * 
 * 每架无人机继承此类，管理自己的时隙调度与优先级队列
 */

#ifndef TDMA_CONTROL_MAC_H
#define TDMA_CONTROL_MAC_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ns3/traced-callback.h"
#include <queue>
#include <list>
#include <map>

namespace ns3 {

/**
 * TDMA帧参数（文档5.3.1节表5-2）：
 *   帧周期    = 30ms
 *   总时隙数  = 36（30上行+4下行+2保护）
 *   每时隙    ≈ 750μs（30ms / 36 - 保护间隔）
 *   保护间隔  ≥ 50μs
 *   载荷      ≤ 256字节
 *   空中速率  ≈ 200kbps (GFSK, 200ksps)
 */
struct TdmaFrameParams {
    uint32_t framePeriodUs   = 30000;  // 30ms
    uint32_t numSlots        = 36;
    uint32_t uplinkSlots     = 30;     // 0~29
    uint32_t downlinkSlots   = 4;      // 30~33
    uint32_t guardSlots      = 2;      // 34~35
    uint32_t guardIntervalUs = 50;     // ≥50μs
    uint32_t slotDurationUs  = 750;    // ~750μs（含保护）
    uint32_t dataDurationUs  = 700;    // 实际数据传输时间
    uint32_t maxPayloadBytes = 256;
};

/**
 * 指令优先级（文档5.3.3节表5-3）
 */
enum PriorityLevel : uint8_t {
    P0_EMERGENCY     = 0,   // 紧急消解指令
    P1_SAFETY        = 1,   // 安全相关指令
    P2_SCHEDULING    = 2,   // 调度指令（航迹更新等）
    P3_AIRSPACE      = 3,   // 空域配置指令
    P4_HEARTBEAT     = 4,   // 心跳与维护
    NUM_PRIORITIES   = 5
};

/**
 * 调度指令类型（文档5.3.3节）
 */
enum CommandType : uint8_t {
    CMD_EMERGENCY_STOP  = 0,
    CMD_AVOID           = 1,
    CMD_RETURN_HOME     = 2,
    CMD_WAYPOINT_UPDATE = 3,
    CMD_SPEED_ADJUST    = 4,
    CMD_HEIGHT_ADJUST   = 5,
    CMD_HOVER           = 6,
    CMD_NO_FLY_ZONE     = 7,
    CMD_HEARTBEAT       = 8,
};

/**
 * TDMA MAC层
 */
class TdmaControlMac : public Object
{
public:
    static TypeId GetTypeId(void);
    TdmaControlMac();
    virtual ~TdmaControlMac();

    // ===== 配置（文档5.3.1节）=====
    void SetNodeId(uint32_t id);
    void SetIsClusterHead(bool isCh);
    void SetIsBaseStation(bool isBs);
    void SetClusterSize(uint32_t size);
    void SetAddress(Address addr);
    void SetLocalPort(uint16_t port);
    void SetRemoteAddress(Address addr);
    void SetRemotePort(uint16_t port);

    // ===== 启停 =====
    void Start(Time simStart);
    void Stop(void);

    // ===== 指令发送接口 =====
    // 上层（调度/空管）通过此接口下发指令
    bool EnqueueCommand(CommandType type, PriorityLevel pri, 
                        Ptr<Packet> payload, uint32_t targetId);

    // ===== 帧结构查询 =====
    uint32_t GetCurrentSlot(void) const;
    void SendBeacon(void);                     // 发送同步信标（public: main可调用）
    Time GetTimeToNextSlot(uint32_t slotId) const;
    TdmaFrameParams GetFrameParams(void) const { return m_frameParams; }

    // ===== Trace（供主程序连接统计回调）=====
    TracedCallback<Time, uint32_t, uint32_t> m_ulTxTrace;   
    // (delay, srcId, slotId) — 上行发送
    TracedCallback<Time, uint32_t, uint32_t> m_dlTxTrace;   
    // (delay, dstId, pri) — 下行发送
    TracedCallback<Time, uint32_t, PriorityLevel> m_rxTrace; 
    // (delay, seq, pri) — 接收完成

    // ===== 延迟预算跟踪（文档5.3.4节）=====
    struct LatencyBudget {
        Time groundProcessing;     // 地面处理段 ≤2ms
        Time queuing;              // 排队段 ≤30ms
        Time transmission;         // 传输/中继段 ≤10ms
        Time airborneProcessing;   // 机载接收段 ≤8ms
        Time total;                // 合计 ≤50ms
    };

private:
    // ===== 时隙调度 =====
    void ScheduleNextSlotBoundary(void);
    void OnSlotStart(void);
    void OnSlotEnd(void);
    
    // 上行处理（无人机→基站）
    void ProcessUplinkSlot(void);
    // 下行处理（基站→无人机）
    void ProcessDownlinkSlot(void);

    // ===== 指令队列（文档5.3.3节）=====
    struct CommandEntry {
        CommandType type;
        PriorityLevel priority;
        Ptr<Packet> payload;
        uint32_t targetId;
        Time enqueueTime;
        uint32_t seqNumber;
    };
    // 五级优先级队列（P0最高）
    std::queue<CommandEntry> m_priorityQueues[NUM_PRIORITIES];
    uint32_t m_seqCounter;

    // 紧急插入：P0指令跳过队列直接调度
    void InsertEmergency(CommandEntry entry);

    // ===== 时钟同步（文档5.3.2节）=====
    Time m_simStartTime;          // 仿真对齐起始时间
    Time m_clockOffset;           // 时钟偏移（简化为固定偏移）
    double m_driftPpm;            // 时钟漂移 (ppm)

    // ===== 帧参数 =====
    TdmaFrameParams m_frameParams;

    // ===== 角色 =====
    uint32_t m_nodeId;
    bool m_isClusterHead;
    bool m_isBaseStation;
    uint32_t m_clusterSize;       // 簇内成员数
    uint32_t m_myUplinkSlotId;    // 本节点上行时隙ID 0~29
    uint32_t m_myDownlinkSlotId;  // 基站下行时隙 30~33

    // ===== 通信 =====
    Ptr<Socket> m_socket;
    Address m_localAddress;
    Address m_remoteAddress;
    uint16_t m_localPort;
    uint16_t m_remotePort;

    // ===== 半动态时隙分配（文档5.3.1节）=====
    std::vector<bool> m_slotAllocation;    // true = 已分配
    std::vector<uint32_t> m_slotOwner;     // 该时隙所属节点ID
    bool UpdateSlotAllocation(void);

    // ===== 事件 =====
    EventId m_slotStartEvent;
    EventId m_slotEndEvent;
    bool m_running;

    // ===== 同步信标（文档5.3.2节）=====
    void ProcessBeacon(Ptr<Socket> socket);
    Time m_lastBeaconTime;
    Time m_beaconInterval;        // 300ms（10帧一个超帧）

    // ===== CRC-16 CCITT（文档5.3.1节：载荷含CRC-16校验）=====
    static uint16_t ComputeCrc16(const uint8_t* data, uint32_t len);
    static bool VerifyCrc16(const uint8_t* data, uint32_t len);

    // ===== 统计 =====
    uint64_t m_txCount;
    uint64_t m_rxCount;
    uint64_t m_dropCount[NUM_PRIORITIES];
    std::vector<LatencyBudget> m_latencyRecords;
};

} // namespace ns3

#endif // TDMA_CONTROL_MAC_H
