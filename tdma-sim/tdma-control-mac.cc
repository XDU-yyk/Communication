/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "tdma-control-mac.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include <numeric>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("TdmaControlMac");
NS_OBJECT_ENSURE_REGISTERED(TdmaControlMac);

TypeId
TdmaControlMac::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::TdmaControlMac")
        .SetParent<Object>()
        .SetGroupName("Network")
        .AddConstructor<TdmaControlMac>()
        .AddAttribute("NodeId", "Node identifier",
                      UintegerValue(0),
                      MakeUintegerAccessor(&TdmaControlMac::SetNodeId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("IsClusterHead", "Whether this node is cluster head",
                      BooleanValue(false),
                      MakeBooleanAccessor(&TdmaControlMac::m_isClusterHead),
                      MakeBooleanChecker())
        .AddAttribute("IsBaseStation", "Whether this node is base station",
                      BooleanValue(false),
                      MakeBooleanAccessor(&TdmaControlMac::m_isBaseStation),
                      MakeBooleanChecker())
    ;
    return tid;
}

TdmaControlMac::TdmaControlMac()
    : m_driftPpm(1.0),
      m_nodeId(0),
      m_isClusterHead(false),
      m_isBaseStation(false),
      m_clusterSize(0),
      m_seqCounter(0),
      m_myUplinkSlotId(0),
      m_myDownlinkSlotId(30),
      m_localPort(9),
      m_remotePort(9),
      m_running(false),
      m_beaconInterval(MilliSeconds(300)),
      m_txCount(0),
      m_rxCount(0)
{
    for (int i = 0; i < NUM_PRIORITIES; i++)
        m_dropCount[i] = 0;

    m_slotAllocation.resize(m_frameParams.numSlots, false);
    m_slotOwner.resize(m_frameParams.numSlots, 0);
}

TdmaControlMac::~TdmaControlMac()
{
}

void
TdmaControlMac::SetNodeId(uint32_t id) 
{ 
    m_nodeId = id; 
    // 文档5.3.1节：30架无人机占上行时隙0~29
    if (id >= 1 && id <= 30)
        m_myUplinkSlotId = id - 1;  // 无人机1→槽0, 无人机30→槽29
}

void
TdmaControlMac::SetIsClusterHead(bool isCh) { m_isClusterHead = isCh; }
void
TdmaControlMac::SetIsBaseStation(bool isBs) { m_isBaseStation = isBs; }
void
TdmaControlMac::SetClusterSize(uint32_t size) { m_clusterSize = size; }

void
TdmaControlMac::SetAddress(Address addr) { m_localAddress = addr; }
void
TdmaControlMac::SetLocalPort(uint16_t port) { m_localPort = port; }
void
TdmaControlMac::SetRemoteAddress(Address addr) { m_remoteAddress = addr; }
void
TdmaControlMac::SetRemotePort(uint16_t port) { m_remotePort = port; }

void
TdmaControlMac::Start(Time simStart)
{
    m_simStartTime = simStart;
    m_running = true;

    // 创建UDP socket
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetObject<Node>(), 
                                         UdpSocketFactory::GetTypeId());
        m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_localPort));
        m_socket->SetRecvCallback(MakeCallback(&TdmaControlMac::ProcessBeacon, this));
    }

    // 半动态时隙初始化：本节点申请自己的上行时隙
    m_slotAllocation[m_myUplinkSlotId] = true;
    m_slotOwner[m_myUplinkSlotId] = m_nodeId;

    // 基站独占下行时隙
    if (m_isBaseStation)
    {
        for (uint32_t s = 30; s < 34; s++)
        {
            m_slotAllocation[s] = true;
            m_slotOwner[s] = 0;  // 基站ID=0
        }
    }

    // 调度第一个时隙边界
    ScheduleNextSlotBoundary();

    // 基站首次发送信标
    if (m_isBaseStation)
        SendBeacon();

    NS_LOG_INFO("TdmaMac[Node" << m_nodeId << "] started, "
                << "uplinkSlot=" << m_myUplinkSlotId
                << ", isBS=" << m_isBaseStation);
}

void
TdmaControlMac::Stop(void)
{
    m_running = false;
    Simulator::Cancel(m_slotStartEvent);
    Simulator::Cancel(m_slotEndEvent);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = 0;
    }
}

// ============================================================
// 时隙调度（文档5.3.1节）
// ============================================================

void
TdmaControlMac::ScheduleNextSlotBoundary(void)
{
    if (!m_running) return;

    // 计算距下一个时隙边界的绝对时间
    Time now = Simulator::Now();
    Time elapsed = now - m_simStartTime;
    int64_t elapsedUs = elapsed.GetMicroSeconds();

    int64_t posInFrame = elapsedUs % m_frameParams.framePeriodUs;
    int64_t slotBoundaryUs = (posInFrame / m_frameParams.slotDurationUs + 1) 
                             * m_frameParams.slotDurationUs;
    int64_t delayToBoundary = slotBoundaryUs - posInFrame;

    // 加上时钟漂移
    double driftCorrection = m_driftPpm * 1e-6 * delayToBoundary;
    delayToBoundary += static_cast<int64_t>(driftCorrection);

    if (delayToBoundary < 10) delayToBoundary = m_frameParams.slotDurationUs;

    m_slotStartEvent = Simulator::Schedule(
        MicroSeconds(delayToBoundary),
        &TdmaControlMac::OnSlotStart, this);
}

void
TdmaControlMac::OnSlotStart(void)
{
    if (!m_running) return;

    uint32_t slotId = GetCurrentSlot();

    if (slotId < m_frameParams.uplinkSlots)
    {
        // 上行时隙 0~29
        ProcessUplinkSlot();
    }
    else if (slotId >= 30 && slotId < 34)
    {
        // 下行时隙 30~33（仅基站处理）
        if (m_isBaseStation)
            ProcessDownlinkSlot();
    }
    // 保护时隙 34~35：仅做同步维护，无数据收发

    // 调度时隙结束事件
    m_slotEndEvent = Simulator::Schedule(
        MicroSeconds(m_frameParams.dataDurationUs),
        &TdmaControlMac::OnSlotEnd, this);
}

void
TdmaControlMac::OnSlotEnd(void)
{
    if (!m_running) return;
    // 调度下一时隙边界
    ScheduleNextSlotBoundary();
}

// ============================================================
// 上行处理（文档5.3.1节：每架无人机在自己的上行时隙发送）
// ============================================================

void
TdmaControlMac::ProcessUplinkSlot(void)
{
    uint32_t slotId = GetCurrentSlot();

    // 是否为我的上行时隙？
    if (slotId != m_myUplinkSlotId)
        return;

    // 从最高优先级队列取包发送
    for (int pri = 0; pri < NUM_PRIORITIES; pri++)
    {
        if (!m_priorityQueues[pri].empty())
        {
            CommandEntry entry = m_priorityQueues[pri].front();
            m_priorityQueues[pri].pop();

            // 构造发出包（前8字节时间戳用于收端测时延）
            uint64_t nowUs = Simulator::Now().GetMicroSeconds();
            uint8_t buf[8];
            memcpy(buf, &nowUs, 8);
            Ptr<Packet> tsPacket = Create<Packet>(buf, 8);

            // 序列号+优先级+目标ID（4字节）
            uint8_t hdr[4];
            hdr[0] = (entry.seqNumber >> 24) & 0xFF;
            hdr[1] = (entry.seqNumber >> 16) & 0xFF;
            hdr[2] = (entry.priority << 4) | (entry.type & 0x0F);
            hdr[3] = entry.targetId;
            Ptr<Packet> hdrPacket = Create<Packet>(hdr, 4);
            tsPacket->AddAtEnd(hdrPacket);

            // 原始载荷
            if (entry.payload)
                tsPacket->AddAtEnd(entry.payload);

            // 附加 CRC-16（文档5.3.1节：载荷含CRC-16校验）
            {
                uint32_t pktLen = tsPacket->GetSize();
                uint8_t* rawData = new uint8_t[pktLen + 2];
                tsPacket->CopyData(rawData, pktLen);
                uint16_t crc = ComputeCrc16(rawData, pktLen);
                rawData[pktLen] = (crc >> 8) & 0xFF;
                rawData[pktLen + 1] = crc & 0xFF;
                tsPacket = Create<Packet>(rawData, pktLen + 2);
                delete[] rawData;
            }

            // 限制包长 ≤256字节
            if (tsPacket->GetSize() > m_frameParams.maxPayloadBytes + 2)
                tsPacket->RemoveAtEnd(
                    tsPacket->GetSize() - m_frameParams.maxPayloadBytes - 2);

            // 发送
            m_socket->SendTo(tsPacket, 0, m_remoteAddress);
            m_txCount++;

            // 记录延迟
            Time delay = Simulator::Now() - entry.enqueueTime;
            m_ulTxTrace(delay, m_nodeId, slotId);

            // 每时隙只发一个包（匹配调度频率）
            break;
        }
    }
}

// ============================================================
// 下行处理（文档5.3.1节：基站利用4个下行时隙下发指令）
// ============================================================

void
TdmaControlMac::ProcessDownlinkSlot(void)
{
    // 从最高优先级队列取包发送
    for (int pri = 0; pri < NUM_PRIORITIES; pri++)
    {
        if (!m_priorityQueues[pri].empty())
        {
            CommandEntry entry = m_priorityQueues[pri].front();
            m_priorityQueues[pri].pop();

            // 时间戳+包头
            uint64_t nowUs = Simulator::Now().GetMicroSeconds();
            uint8_t buf[8];
            memcpy(buf, &nowUs, 8);
            Ptr<Packet> tsPacket = Create<Packet>(buf, 8);

            uint8_t hdr[4];
            hdr[0] = (entry.seqNumber >> 24) & 0xFF;
            hdr[1] = (entry.seqNumber >> 16) & 0xFF;
            hdr[2] = (entry.priority << 4) | (entry.type & 0x0F);
            hdr[3] = entry.targetId;
            Ptr<Packet> hdrPacket = Create<Packet>(hdr, 4);
            tsPacket->AddAtEnd(hdrPacket);

            if (entry.payload)
                tsPacket->AddAtEnd(entry.payload);

            // 附加 CRC-16
            {
                uint32_t pktLen = tsPacket->GetSize();
                uint8_t* rawData = new uint8_t[pktLen + 2];
                tsPacket->CopyData(rawData, pktLen);
                uint16_t crc = ComputeCrc16(rawData, pktLen);
                rawData[pktLen] = (crc >> 8) & 0xFF;
                rawData[pktLen + 1] = crc & 0xFF;
                tsPacket = Create<Packet>(rawData, pktLen + 2);
                delete[] rawData;
            }

            if (tsPacket->GetSize() > m_frameParams.maxPayloadBytes + 2)
                tsPacket->RemoveAtEnd(
                    tsPacket->GetSize() - m_frameParams.maxPayloadBytes - 2);

            m_socket->SendTo(tsPacket, 0, m_remoteAddress);
            m_txCount++;

            Time delay = Simulator::Now() - entry.enqueueTime;
            m_dlTxTrace(delay, entry.targetId, entry.priority);
            break;
        }
    }
}

// ============================================================
// 指令入队（文档5.3.3节：五级优先级）
// ============================================================

bool
TdmaControlMac::EnqueueCommand(CommandType type, PriorityLevel pri,
                                Ptr<Packet> payload, uint32_t targetId)
{
    if (!m_running) return false;

    CommandEntry entry;
    entry.type        = type;
    entry.priority    = pri;
    entry.payload     = payload;
    entry.targetId    = targetId;
    entry.enqueueTime = Simulator::Now();
    entry.seqNumber   = ++m_seqCounter;

    if (pri == P0_EMERGENCY)
    {
        // P0: 跳过队列，调度接口直接插入到队列头部（文档5.3.3节）
        InsertEmergency(entry);
    }
    else
    {
        // P1~P4: 按优先级入队
        if (m_priorityQueues[pri].size() < 200)  // 防溢出
            m_priorityQueues[pri].push(entry);
        else
            m_dropCount[pri]++;
    }
    return true;
}

void
TdmaControlMac::InsertEmergency(CommandEntry entry)
{
    // 创建一个临时队列，将紧急指令放在最前面
    std::queue<CommandEntry> temp;
    temp.push(entry);
    while (!m_priorityQueues[P0_EMERGENCY].empty())
    {
        temp.push(m_priorityQueues[P0_EMERGENCY].front());
        m_priorityQueues[P0_EMERGENCY].pop();
    }
    m_priorityQueues[P0_EMERGENCY] = temp;
}

// ============================================================
// 信标同步（文档5.3.2节）
// ============================================================

void
TdmaControlMac::SendBeacon(void)
{
    if (!m_running) return;

    // 基站发送信标（含当前帧计数器和时隙计数器）
    uint64_t elapsedUs = (Simulator::Now() - m_simStartTime).GetMicroSeconds();
    uint32_t frameCount = elapsedUs / m_frameParams.framePeriodUs;
    uint32_t slotCount = (elapsedUs % m_frameParams.framePeriodUs) 
                         / m_frameParams.slotDurationUs;

    uint8_t bcnData[12];
    memcpy(bcnData, &frameCount, 4);
    memcpy(bcnData + 4, &slotCount, 4);
    memcpy(bcnData + 8, &m_nodeId, 4);

    Ptr<Packet> beacon = Create<Packet>(bcnData, 12);
    m_socket->SendTo(beacon, 0, m_remoteAddress);

    // 下一信标
    Simulator::Schedule(m_beaconInterval, 
                        &TdmaControlMac::SendBeacon, this);
}

void
TdmaControlMac::ProcessBeacon(Ptr<Socket> socket)
{
    Ptr<Packet> pkt;
    Address from;
    while ((pkt = m_socket->RecvFrom(from)))
    {
        uint32_t pktSize = pkt->GetSize();

        // CRC-16 校验（文档5.3.1节：载荷含CRC-16）
        // 信标12B无CRC，数据包≥14B才有（12B包头+2B CRC）
        if (pktSize >= 14)
        {
            uint8_t* raw = new uint8_t[pktSize];
            pkt->CopyData(raw, pktSize);
            if (!VerifyCrc16(raw, pktSize))
            {
                m_dropCount[P4_HEARTBEAT]++;  // CRC错误计入统计
                delete[] raw;
                continue;
            }
            delete[] raw;
        }

        // 信标处理（文档5.3.2节）
        if (pktSize >= 12)
        {
            uint8_t data[12];
            pkt->CopyData(data, 12);

            uint32_t frameCount, slotCount, srcId;
            memcpy(&frameCount, data, 4);
            memcpy(&slotCount, data + 4, 4);
            memcpy(&srcId, data + 8, 4);

            // 收到信标 → 重置时钟偏移（仿真中设为完全同步）
            m_clockOffset = Simulator::Now() - m_simStartTime 
                          - MicroSeconds(frameCount * m_frameParams.framePeriodUs 
                                          + slotCount * m_frameParams.slotDurationUs);

            // 收到基站信标 → 成员更新时隙对齐
            if (srcId == 0)
            {
                m_simStartTime = Simulator::Now() - 
                    MicroSeconds(frameCount * m_frameParams.framePeriodUs
                                  + slotCount * m_frameParams.slotDurationUs);
            }
        }
    }
}

// ============================================================
// 辅助函数
// ============================================================

uint32_t
TdmaControlMac::GetCurrentSlot(void) const
{
    Time elapsed = Simulator::Now() - m_simStartTime;
    int64_t posInFrame = elapsed.GetMicroSeconds() % m_frameParams.framePeriodUs;
    return static_cast<uint32_t>(posInFrame / m_frameParams.slotDurationUs);
}

Time
TdmaControlMac::GetTimeToNextSlot(uint32_t slotId) const
{
    Time now = Simulator::Now();
    Time elapsed = now - m_simStartTime;
    int64_t elapsedUs = elapsed.GetMicroSeconds();
    int64_t posInFrame = elapsedUs % m_frameParams.framePeriodUs;
    int64_t currentSlot = posInFrame / m_frameParams.slotDurationUs;

    int64_t delay;
    if (slotId > currentSlot)
        delay = (slotId - currentSlot) * m_frameParams.slotDurationUs
                - (posInFrame % m_frameParams.slotDurationUs);
    else
        delay = m_frameParams.framePeriodUs - posInFrame
                + slotId * m_frameParams.slotDurationUs;

    return MicroSeconds(delay);
}

// ============================================================
// CRC-16 CCITT（文档5.3.1节）
// 多项式: x^16 + x^12 + x^5 + 1 (0x1021)
// ============================================================

uint16_t
TdmaControlMac::ComputeCrc16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

bool
TdmaControlMac::VerifyCrc16(const uint8_t* data, uint32_t len)
{
    if (len < 2) return false;
    uint16_t computed = ComputeCrc16(data, len - 2);
    uint16_t received = ((uint16_t)data[len - 2] << 8) | data[len - 1];
    return computed == received;
}

bool
TdmaControlMac::UpdateSlotAllocation(void)
{
    // 文档5.3.1节：半动态时隙分配
    // 预留池：槽24~29共6个时隙用于新入网节点
    static const uint32_t RESERVE_POOL_START = 24;
    static const uint32_t RESERVE_POOL_SIZE  = 6;

    // 1. 释放离线节点的时隙（归还预留池）
    for (uint32_t s = 0; s < RESERVE_POOL_START; s++)
    {
        if (m_slotAllocation[s] && m_slotOwner[s] > 0)
        {
            // 简化：不追踪在线状态，保持分配
            // 扩展时此处添加心跳超时检测
        }
    }

    // 2. 从预留池为新节点分配时隙
    for (uint32_t p = 0; p < RESERVE_POOL_SIZE; p++)
    {
        uint32_t slot = RESERVE_POOL_START + p;
        if (!m_slotAllocation[slot])
        {
            // 预留池有空位，等待新节点加入
            // 扩展时此处接收簇头分配的slot
        }
    }

    // 3. 预留池耗尽→触发帧重分配（文档5.3.1节末尾）
    // 扩展时此处实现：增加时隙数/压缩单时隙/分裂子簇
    uint32_t usedReserve = 0;
    for (uint32_t p = 0; p < RESERVE_POOL_SIZE; p++)
        if (m_slotAllocation[RESERVE_POOL_START + p]) usedReserve++;

    if (usedReserve >= RESERVE_POOL_SIZE)
    {
        NS_LOG_WARN("Reservation pool exhausted (" << usedReserve
                     << "/" << RESERVE_POOL_SIZE
                     << "), frame reallocation required");
    }

    return true;
}

} // namespace ns3
