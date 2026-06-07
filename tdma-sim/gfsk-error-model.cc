/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "gfsk-error-model.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("GfskErrorModel");
NS_OBJECT_ENSURE_REGISTERED(GfskErrorModel);

TypeId
GfskErrorModel::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::GfskErrorModel")
        .SetParent<ErrorModel>()
        .SetGroupName("Network")
        .AddConstructor<GfskErrorModel>()
        .AddAttribute("ModulationIndex",
                      "GFSK modulation index h (default 0.5)",
                      DoubleValue(0.5),
                      MakeDoubleAccessor(&GfskErrorModel::m_modulationIndex),
                      MakeDoubleChecker<double>(0.1, 2.0))
        .AddAttribute("BandwidthTime",
                      "Bandwidth-time product BT (default 0.5)",
                      DoubleValue(0.5),
                      MakeDoubleAccessor(&GfskErrorModel::m_bandwidthTime),
                      MakeDoubleChecker<double>(0.1, 2.0))
        .AddAttribute("SymbolRate",
                      "Symbol rate in ksps (default 200)",
                      DoubleValue(200e3),
                      MakeDoubleAccessor(&GfskErrorModel::m_symbolRate),
                      MakeDoubleChecker<double>(1e3, 10e6))
        .AddAttribute("Coherent",
                      "Use coherent demodulation (default false)",
                      BooleanValue(false),
                      MakeBooleanAccessor(&GfskErrorModel::m_coherent),
                      MakeBooleanChecker())
    ;
    return tid;
}

GfskErrorModel::GfskErrorModel()
    : m_modulationIndex(0.5),
      m_bandwidthTime(0.5),
      m_symbolRate(200e3),
      m_isiFactor(0.8),
      m_coherent(false),
      m_totalBits(0),
      m_totalErrors(0)
{
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(1.0));
}

GfskErrorModel::~GfskErrorModel()
{
}

void
GfskErrorModel::SetModulationIndex(double h)       { m_modulationIndex = h; }
void
GfskErrorModel::SetBandwidthTimeProduct(double BT) { m_bandwidthTime = BT; }
void
GfskErrorModel::SetSymbolRate(double Rs)           { m_symbolRate = Rs; }
void
GfskErrorModel::SetDetectionMode(bool coherent)    { m_coherent = coherent; }

double
GfskErrorModel::ComputeBer(double sinrDb) const
{
    // SINR (dB) → Eb/N0 (线性)
    // 对于 GFSK BT=0.5，99% 带宽 ≈ (1 + h) * Rs = 1.5 * 200kHz = 300kHz
    // 但实际占用的 -3dB 带宽 ≈ Rs * (0.5 + BT) ≈ 200k * 1.0 = 200kHz
    // Eb/N0 = SNR_linear * (BW / Rb)，其中 Rb = Rs（二电平GFSK每符号1bit）
    double bw_hz = m_symbolRate;  // -3dB 带宽近似 = Rs
    double rb_bps = m_symbolRate; // 1 bit/symbol for binary GFSK
    double sinrLinear = std::pow(10.0, sinrDb / 10.0);
    double ebNo = sinrLinear * (bw_hz / rb_bps);

    // ISI 惩罚因子
    double effectiveEbNo = ebNo * m_isiFactor;

    double ber;
    if (m_coherent)
    {
        // 相干 GFSK: BER = 0.5 * erfc(sqrt(Eb/N0 / 2))
        ber = 0.5 * std::erfc(std::sqrt(effectiveEbNo / 2.0));
    }
    else
    {
        // 非相干 GFSK: BER ≈ 0.5 * exp(-Eb/N0 / 2)
        // 高斯预滤波引入的 ISI 使有效 Eb/N0 下降约 0.5-1dB
        ber = 0.5 * std::exp(-effectiveEbNo / 2.0);
    }

    // 限幅：BER ∈ [0, 0.5]
    return std::max(0.0, std::min(0.5, ber));
}

bool
GfskErrorModel::DoCorrupt(Ptr<Packet> p)
{
    if (!IsEnabled()) return false;

    // 从属性中获取当前 SINR（由调用者设置，这里用固定值模拟）
    // 实际 NS3 中由 ErrorModel 框架通过 Rx 对象传递
    // 简化实现：使用随机 BER 模拟（0.001 ~ 0.1 对应 SNR 10~20dB 范围）
    double ber = 0.001;  // 默认低错误率

    uint32_t pktSizeBits = p->GetSize() * 8;
    m_totalBits += pktSizeBits;

    // 逐位判断是否出错
    bool corrupted = false;
    for (uint32_t b = 0; b < pktSizeBits; b++)
    {
        if (m_random->GetValue() < ber)
        {
            corrupted = true;
            m_totalErrors++;
        }
    }

    if (corrupted)
    {
        NS_LOG_DEBUG("GFSK: packet corrupted, BER=" << ber
                      << " size=" << p->GetSize() << "B");
    }

    return corrupted;
}

void
GfskErrorModel::DoReset(void)
{
    m_totalBits = 0;
    m_totalErrors = 0;
}

} // namespace ns3
