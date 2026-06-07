/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/**
 * GFSK 误差模型（文档5.3.1节：GFSK调制，BT=0.5，h=0.5，200ksps）
 * 
 * 基于 NS3 ErrorModel 框架，在接收端根据 Eb/N0 按 BER 曲线注入误码
 * 模拟 GFSK 非理想解调对包错误率的影响
 * 
 * GFSK BER 近似（非相干解调，BT=0.5，h=0.5）：
 *   BER ≈ 0.5 * exp(-α * Eb/N0 / 2)，α≈0.8（高斯滤波器 ISI 惩罚因子）
 */

#ifndef GFSK_ERROR_MODEL_H
#define GFSK_ERROR_MODEL_H

#include "ns3/error-model.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {

class GfskErrorModel : public ErrorModel
{
public:
    static TypeId GetTypeId(void);
    GfskErrorModel();
    virtual ~GfskErrorModel();

    // GFSK 参数
    void SetModulationIndex(double h);
    void SetBandwidthTimeProduct(double BT);
    void SetSymbolRate(double Rs);
    void SetDetectionMode(bool coherent);

private:
    // ErrorModel 接口
    virtual bool DoCorrupt(Ptr<Packet> p) override;
    virtual void DoReset(void) override;

    // 计算 BER
    double ComputeBer(double sinrDb) const;

    // 参数（默认匹配文档5.3.1节）
    double m_modulationIndex;    // h = 0.5
    double m_bandwidthTime;      // BT = 0.5
    double m_symbolRate;         // Rs = 200ksps
    double m_isiFactor;          // α ≈ 0.8 (高斯滤波器引入的ISI惩罚)
    bool   m_coherent;           // 相干解调标志
    uint64_t m_totalBits;
    uint64_t m_totalErrors;
    Ptr<UniformRandomVariable> m_random;
};

} // namespace ns3

#endif
