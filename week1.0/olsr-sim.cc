/* -*- Mode: C++; c-basic-offset:2; indent-tabs-mode:nil -*- */
/**
 * OLSR 无人机自组网仿真 — 主程序
 * 
 * 对应 week1.0/workflow.md
 *   第5章 确定性通信与自组网子系统设计（簇间Mesh路由）
 *   第15章 课题研究成果与交付物（AODV/OLSR仿真比较）
 *
 * 场景（G0 静态基线 + G1 低速移动）：
 *   30 架无人机在 10km 走廊内，5 个簇头候选节点
 *   1 个地面基站
 *   OLSR 路由 + WiFi AdHoc（多跳）
 *   UDP 状态流量
 *   FlowMonitor 采集
 *
 * 运行（Ubuntu VM）：
 *   cd ns-3.43
 *   ./ns3 configure --enable-examples --enable-tests
 *   ./ns3 build
 *   ./ns3 run "scratch/olsr-sim --scenario=static --nUavs=30 --simTime=300"
 *
 * 参数：
 *   scenario  场景：static（默认）| mobility（5-15 m/s 路点移动）
 *   nUavs     无人机数量（默认 30）
 *   simTime   仿真时长秒（默认 300）
 *   seed      随机种子（默认 1）
 *   resultsDir 输出目录（默认 "results"）
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OlsrSim");

// ============================================================
// 全局统计和输出
// ============================================================
struct RunConfig {
    std::string scenario;
    uint32_t nUavs;
    double simTime;
    uint32_t seed;
    std::string resultsDir;
    uint32_t nClusterHeads;
    double corridorLength;    // 米
    double corridorWidth;     // 米
    double uavAltitude;      // 米
    double wifiRange;        // 米
    double helloIntervalSec; // OLSR Hello 间隔
    double tcIntervalSec;    // OLSR TC 间隔
};

// 节点角色
enum NodeRole { GROUND_STATION = 0, CLUSTER_HEAD, ORDINARY_UAV };

struct NodeInfo {
    NodeRole role;
    uint32_t clusterId; // 所属簇
    uint32_t nodeId;    // ns-3 ID
};

// 路由开销统计
struct RoutingOverhead {
    uint64_t helloCount = 0;
    uint64_t helloBytes = 0;
    uint64_t tcCount = 0;
    uint64_t tcBytes = 0;
    uint64_t totalRoutingBytes = 0;
};

RunConfig g_config;
std::vector<NodeInfo> g_nodeInfo;
RoutingOverhead g_overhead;
std::ofstream g_routeLog;

// ============================================================
// 参数冻结（对应 notes/scenario-parameters.md）
// ============================================================
static RunConfig GetDefaultConfig()
{
    RunConfig c;
    c.scenario = "static";
    c.nUavs = 30;
    c.simTime = 300.0;
    c.seed = 1;
    c.resultsDir = "results";
    c.nClusterHeads = 5;
    c.corridorLength = 10000.0;  // 10 km
    c.corridorWidth = 500.0;     // ±250 m
    c.uavAltitude = 100.0;       // 100 m
    c.wifiRange = 2000.0;        // ~2 km → 多跳
    c.helloIntervalSec = 2.0;    // OLSR 默认
    c.tcIntervalSec = 5.0;       // OLSR 默认
    return c;
}

// ============================================================
// 节点部署
// ============================================================
static void DeployNodes(NodeContainer &uavNodes, NodeContainer &bsNode,
                          Ptr<ListPositionAllocator> posAlloc,
                          std::vector<uint32_t> &clusterHeadIds)
{
    double len = g_config.corridorLength;
    double halfW = g_config.corridorWidth / 2.0;
    double alt = g_config.uavAltitude;
    double spacing = len / (g_config.nUavs + 1);

    // 地面站位于走廊起点
    posAlloc->Add(Vector(0.0, 0.0, 0.0));

    // 簇头 ID：等距分布在 10% / 27.5% / 45% / 62.5% / 80% 位置
    double chPositions[] = {0.10, 0.275, 0.45, 0.625, 0.80};
    bool isClusterHead[30] = {false};
    for (int i = 0; i < g_config.nClusterHeads && i < 5; i++) {
        int idx = static_cast<int>(chPositions[i] * g_config.nUavs);
        if (idx >= (int)g_config.nUavs) idx = g_config.nUavs - 1;
        isClusterHead[idx] = true;
        clusterHeadIds.push_back(idx);
    }

    // UAV 节点
    for (uint32_t i = 0; i < g_config.nUavs; i++) {
        double x = (i + 1) * spacing;
        double y = (i % 2 == 0) ? -halfW * 0.5 : halfW * 0.5;
        posAlloc->Add(Vector(x, y, alt));
    }

    // 记录节点角色
    g_nodeInfo.clear();
    g_nodeInfo.push_back({GROUND_STATION, 0, 0}); // 节点 0 = 地面站
    for (uint32_t i = 0; i < g_config.nUavs; i++) {
        NodeRole role = isClusterHead[i] ? CLUSTER_HEAD : ORDINARY_UAV;
        uint32_t clusterId = 0;
        for (int c = 0; c < g_config.nClusterHeads && c < 5; c++) {
            if (i == clusterHeadIds[c]) {
                clusterId = c + 1;
                break;
            }
        }
        g_nodeInfo.push_back({role, clusterId, i + 1}); // ns-3 node ID = i+1
    }
}

// ============================================================
// 移动模型
// ============================================================
static void SetupMobility(NodeContainer &uavNodes, NodeContainer &bsNode,
                           Ptr<ListPositionAllocator> posAlloc)
{
    // 地面站固定
    MobilityHelper mobilityBs;
    mobilityBs.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityBs.Install(bsNode);

    if (g_config.scenario == "mobility") {
        // G1: Waypoint mobility 5-15 m/s
        MobilityHelper mobilityUav;
        mobilityUav.SetPositionAllocator(posAlloc);
        mobilityUav.SetMobilityModel("ns3::WaypointMobilityModel");
        mobilityUav.Install(uavNodes);

        // 为每个 UAV 生成随机路点路径
        double halfW = g_config.corridorWidth / 2.0;
        double alt = g_config.uavAltitude;
        Ptr<UniformRandomVariable> speedVar = CreateObject<UniformRandomVariable>();
        speedVar->SetAttribute("Min", DoubleValue(5.0));
        speedVar->SetAttribute("Max", DoubleValue(15.0));

        Ptr<UniformRandomVariable> xVar = CreateObject<UniformRandomVariable>();
        xVar->SetAttribute("Min", DoubleValue(0.0));
        xVar->SetAttribute("Max", DoubleValue(g_config.corridorLength));

        Ptr<UniformRandomVariable> yVar = CreateObject<UniformRandomVariable>();
        yVar->SetAttribute("Min", DoubleValue(-halfW));
        yVar->SetAttribute("Max", DoubleValue(halfW));

        for (uint32_t i = 0; i < uavNodes.GetN(); i++) {
            Ptr<WaypointMobilityModel> mob = 
                uavNodes.Get(i)->GetObject<WaypointMobilityModel>();
            if (!mob) continue;

            double t = 0.0;
            while (t < g_config.simTime) {
                double speed = speedVar->GetValue();
                double nextX = xVar->GetValue();
                double nextY = yVar->GetValue();
                double dist = sqrt(pow(nextX - mob->GetPosition().x, 2) +
                                   pow(nextY - mob->GetPosition().y, 2));
                double dt = dist / speed;
                if (dt < 10.0) dt = 10.0; // 至少 10s 一段
                if (t + dt > g_config.simTime) dt = g_config.simTime - t;
                mob->AddWaypoint(Waypoint(Seconds(t), Vector(nextX, nextY, alt)));
                t += dt;
            }
        }
    } else {
        // G0: 静态
        MobilityHelper mobilityUav;
        mobilityUav.SetPositionAllocator(posAlloc);
        mobilityUav.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityUav.Install(uavNodes);
    }
}

// ============================================================
// 应用层流量
// ============================================================
static void InstallTraffic(NodeContainer &uavNodes, NodeContainer &bsNode,
                           Ipv4InterfaceContainer &ifaces,
                           std::vector<uint32_t> &clusterHeadIds)
{
    uint16_t port = 9;
    Ipv4Address bsAddr = ifaces.GetAddress(0);

    // 在地面站装 sink
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(bsNode);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(g_config.simTime));

    // 簇头 → 地面站 UDP（1包/10s）
    OnOffHelper onOff("ns3::UdpSocketFactory",
                      InetSocketAddress(bsAddr, port));
    onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOff.SetAttribute("DataRate", DataRateValue(DataRate("20bps")));  // ~1 pkt/10s w/ 256B
    onOff.SetAttribute("PacketSize", UintegerValue(256));

    for (uint32_t id : clusterHeadIds) {
        ApplicationContainer app = onOff.Install(uavNodes.Get(id));
        app.Start(Seconds(2.0 + id * 0.1));
        app.Stop(Seconds(g_config.simTime));
    }

    // 所有 UAV → 地面站 背景流量（1包/100s）
    OnOffHelper bgOnOff("ns3::UdpSocketFactory",
                        InetSocketAddress(bsAddr, port));
    bgOnOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    bgOnOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    bgOnOff.SetAttribute("DataRate", DataRateValue(DataRate("2bps")));  // ~1 pkt/100s w/ 256B
    bgOnOff.SetAttribute("PacketSize", UintegerValue(128));

    for (uint32_t i = 0; i < uavNodes.GetN(); i++) {
        // 簇头已有流量，不重复添加
        bool isCh = false;
        for (uint32_t id : clusterHeadIds) {
            if (i == id) { isCh = true; break; }
        }
        if (isCh) continue;

        ApplicationContainer app = bgOnOff.Install(uavNodes.Get(i));
        app.Start(Seconds(2.5 + i * 0.05));
        app.Stop(Seconds(g_config.simTime));
    }
}

// ============================================================
// 路由表快照回调
// ============================================================
static void PrintRouteTable(Ptr<OutputStreamWrapper> stream,
                             Ptr<Ipv4> ipv4, Ptr<Ipv4RoutingProtocol> rp)
{
    *stream->GetStream() << "=== Route Snapshot at t=" 
                          << Simulator::Now().GetSeconds() << "s ===" << std::endl;
    rp->PrintRoutingTable(stream);
    *stream->GetStream() << std::endl;
}

static void ScheduleRouteSnapshot(Ptr<Ipv4> ipv4, 
                                   Ptr<OutputStreamWrapper> routeStream)
{
    // 每 60s 打印路由表（至少一次多跳验证）
    static int snapshotCount = 0;
    snapshotCount++;

    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
    PrintRouteTable(routeStream, ipv4, rp);

    if (Simulator::Now().GetSeconds() + 60.0 < g_config.simTime) {
        Simulator::Schedule(Seconds(60.0), &ScheduleRouteSnapshot,
                            ipv4, routeStream);
    }
}

// ============================================================
// 路由开销统计（通过 Tracing）
// ============================================================
static void TxTrace(std::string context, Ptr<const Packet> packet)
{
    // 简单统计：所有发送的路由报文
    // OLSR 报文通常 <= 200B，这里只是粗略估计
    // 更精确需要 OLSR 模块的特定 trace 源
    g_overhead.totalRoutingBytes += packet->GetSize();
}

// ============================================================
// 写摘要 CSV
// ============================================================
static void WriteSummary(Ptr<FlowMonitor> monitor, const std::string &csvPath)
{
    std::ofstream csv(csvPath);
    csv << "=== OLSR Simulation Summary ===" << std::endl;
    csv << "Scenario," << g_config.scenario << std::endl;
    csv << "NUavs," << g_config.nUavs << std::endl;
    csv << "SimTime," << g_config.simTime << "s" << std::endl;
    csv << "Seed," << g_config.seed << std::endl;
    csv << "WifiRange," << g_config.wifiRange << "m" << std::endl;
    csv << "OLSR_HelloInterval," << g_config.helloIntervalSec << "s" << std::endl;
    csv << "OLSR_TcInterval," << g_config.tcIntervalSec << "s" << std::endl;
    csv << std::endl;

    // FlowMonitor 统计
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = 
        DynamicCast<Ipv4FlowClassifier>(monitor->GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    csv << "--- Per-Flow Metrics ---" << std::endl;
    csv << "FlowID,SrcAddr,DstAddr,TxPkts,RxPkts,PDR,LostPkts,"
        << "MeanDelay_ms,MaxDelay_ms,Jitter_ms,TxBytes,RxBytes" << std::endl;

    uint64_t totalTx = 0, totalRx = 0;
    double totalDelaySum = 0.0;
    uint64_t totalDelayCount = 0;
    double pdrSum = 0.0;
    uint32_t flowCount = 0;

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double pdr = flow.second.txPackets > 0 
            ? 100.0 * flow.second.rxPackets / flow.second.txPackets : 0.0;

        csv << flow.first << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << flow.second.txPackets << ","
            << flow.second.rxPackets << ","
            << std::fixed << std::setprecision(2) << pdr << "%,"
            << flow.second.lostPackets << ","
            << std::fixed << std::setprecision(4)
            << flow.second.delaySum.GetMilliSeconds() << ","
            << flow.second.maxDelay.GetMilliSeconds() << ","
            << flow.second.jitterSum.GetMilliSeconds() << ","
            << flow.second.txBytes << ","
            << flow.second.rxBytes
            << std::endl;

        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        totalDelaySum += flow.second.delaySum.GetMilliSeconds();
        totalDelayCount += flow.second.rxPackets;
        pdrSum += pdr;
        flowCount++;
    }

    csv << std::endl;
    csv << "--- Aggregate ---" << std::endl;
    csv << "TotalTxPkts," << totalTx << std::endl;
    csv << "TotalRxPkts," << totalRx << std::endl;
    csv << "OverallPDR," << (totalTx > 0 ? 100.0 * totalRx / totalTx : 0.0) << "%" << std::endl;
    csv << "AvgPDR," << (flowCount > 0 ? pdrSum / flowCount : 0.0) << "%" << std::endl;
    csv << "AvgDelay_ms," << (totalDelayCount > 0 ? totalDelaySum / totalDelayCount : 0.0) << std::endl;
    csv << "FlowCount," << flowCount << std::endl;

    csv << std::endl;
    csv << "--- Routing Overhead (approximate) ---" << std::endl;
    csv << "TotalRoutingBytes," << g_overhead.totalRoutingBytes << std::endl;
    csv << "TotalAppBytes," << totalTx << " pkts" << std::endl;
    csv << "OverheadRatio," << g_overhead.totalRoutingBytes 
        << " bytes / application" << std::endl;

    csv.close();
    std::cout << "Summary written to: " << csvPath << std::endl;
}

// ============================================================
// 写运行配置 JSON
// ============================================================
static void WriteConfigJSON(const std::string &jsonPath)
{
    std::ofstream j(jsonPath);
    j << "{" << std::endl;
    j << "  \"scenario\": \"" << g_config.scenario << "\"," << std::endl;
    j << "  \"nUavs\": " << g_config.nUavs << "," << std::endl;
    j << "  \"simTime\": " << g_config.simTime << "," << std::endl;
    j << "  \"seed\": " << g_config.seed << "," << std::endl;
    j << "  \"nClusterHeads\": " << g_config.nClusterHeads << "," << std::endl;
    j << "  \"corridorLength_m\": " << g_config.corridorLength << "," << std::endl;
    j << "  \"corridorWidth_m\": " << g_config.corridorWidth << "," << std::endl;
    j << "  \"uavAltitude_m\": " << g_config.uavAltitude << "," << std::endl;
    j << "  \"wifiRange_m\": " << g_config.wifiRange << "," << std::endl;
    j << "  \"helloInterval_s\": " << g_config.helloIntervalSec << "," << std::endl;
    j << "  \"tcInterval_s\": " << g_config.tcIntervalSec << std::endl;
    j << "}" << std::endl;
    j.close();
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char *argv[])
{
    // ===== 加载默认配置 =====
    g_config = GetDefaultConfig();

    // ===== 命令行参数 =====
    CommandLine cmd;
    cmd.AddValue("scenario", "Scenario: static | mobility", g_config.scenario);
    cmd.AddValue("nUavs", "Number of UAVs (default 30)", g_config.nUavs);
    cmd.AddValue("simTime", "Simulation duration (seconds)", g_config.simTime);
    cmd.AddValue("seed", "Random seed", g_config.seed);
    cmd.AddValue("resultsDir", "Results output directory", g_config.resultsDir);
    cmd.AddValue("wifiRange", "WiFi range in meters", g_config.wifiRange);
    cmd.AddValue("helloInterval", "OLSR Hello interval (seconds)", g_config.helloIntervalSec);
    cmd.AddValue("tcInterval", "OLSR TC interval (seconds)", g_config.tcIntervalSec);
    cmd.Parse(argc, argv);

    // ===== 日志 =====
    LogComponentEnable("OlsrSim", LOG_LEVEL_INFO);

    // ===== 随机种子 =====
    SeedManager::SetSeed(g_config.seed);
    SeedManager::SetRun(g_config.seed);

    // ===== 创建节点 =====
    NodeContainer uavNodes;
    uavNodes.Create(g_config.nUavs);

    NodeContainer bsNode;
    bsNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(bsNode);
    allNodes.Add(uavNodes);

    // ===== 节点部署 =====
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    std::vector<uint32_t> clusterHeadIds;
    DeployNodes(uavNodes, bsNode, posAlloc, clusterHeadIds);

    // ===== WiFi（AdHoc + Range 传播）=====
    // 物理层
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // 使用 Range 模型来控制多跳
    channel.AddPropagationLoss("ns3::RangePropagationLossModel",
                               "MaxRange", DoubleValue(g_config.wifiRange));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // MAC — AdHoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devs;
    devs.Add(phy.Install(wifi, mac, bsNode));
    devs.Add(phy.Install(wifi, mac, uavNodes));

    // ===== 移动模型 =====
    SetupMobility(uavNodes, bsNode, posAlloc);

    // ===== 路由 + Internet 协议栈 =====
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 100);
    list.Add(staticRouting, 0);

    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(bsNode);
    stack.Install(uavNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devs);

    // ===== 流量 =====
    InstallTraffic(uavNodes, bsNode, ifaces, clusterHeadIds);

    // ===== 打印节点角色 =====
    NS_LOG_INFO("Nodes: " << (g_config.nUavs + 1) 
                << " (" << g_config.nUavs << " UAVs + 1 Ground Station)");
    NS_LOG_INFO("Cluster heads: ");
    for (uint32_t id : clusterHeadIds) {
        NS_LOG_INFO("  CH " << (id + 1) << " (Node " << (id + 1) << ")");
    }

    // ===== FlowMonitor =====
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // ===== 路由表快照（验证多跳连通）=====
    AsciiTraceHelper ascii;
    std::string routeLogPath = g_config.resultsDir + "/olsr_routes.log";
    Ptr<OutputStreamWrapper> routeStream = ascii.CreateFileStream(routeLogPath);

    // 从地面站的路由表观察
    Ptr<Ipv4> bsIpv4 = bsNode.Get(0)->GetObject<Ipv4>();
    Simulator::Schedule(Seconds(30.0), &ScheduleRouteSnapshot, bsIpv4, routeStream);

    // 额外从第5个簇头（远端）看路由
    if (clusterHeadIds.size() >= 5) {
        uint32_t farCh = clusterHeadIds[4]; // 最后一个簇头
        Ptr<Ipv4> chIpv4 = uavNodes.Get(farCh)->GetObject<Ipv4>();
        Simulator::Schedule(Seconds(35.0), &ScheduleRouteSnapshot, chIpv4, routeStream);
    }

    // ===== 路由开销追踪 =====
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxEnd",
                    MakeCallback(&TxTrace));

    // ===== 运行 =====
    NS_LOG_INFO("╔══════════════════════════════════════╗");
    NS_LOG_INFO("║  OLSR Mesh Simulation              ║");
    NS_LOG_INFO("║  " << std::setw(8) << g_config.scenario 
                << " | " << std::setw(3) << g_config.nUavs << " UAVs"
                << " | " << g_config.simTime << "s" << "   ║");
    NS_LOG_INFO("║  WiFi range: " << g_config.wifiRange << "m"
                << " | Seeds: " << g_config.seed << "          ║");
    NS_LOG_INFO("╚══════════════════════════════════════╝");

    Simulator::Stop(Seconds(g_config.simTime + 5.0));
    Simulator::Run();

    // ===== 结果输出 =====
    std::string flowmonPath = g_config.resultsDir + "/olsr_flowmon.xml";
    monitor->SerializeToXmlFile(flowmonPath, true, true);

    std::string csvPath = g_config.resultsDir + "/olsr_summary.csv";
    WriteSummary(monitor, csvPath);

    std::string jsonPath = g_config.resultsDir + "/olsr_run_config.json";
    WriteConfigJSON(jsonPath);

    // ===== 汇总打印 =====
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint64_t totalTx = 0, totalRx = 0;
    double totalDelay = 0.0;
    uint64_t delayCount = 0;

    for (auto &flow : stats) {
        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetMilliSeconds();
        delayCount += flow.second.rxPackets;
    }

    std::cout << std::endl;
    std::cout << "========== OLSR Results ==========" << std::endl;
    std::cout << "  Total Tx Packets: " << totalTx << std::endl;
    std::cout << "  Total Rx Packets: " << totalRx << std::endl;
    std::cout << "  Overall PDR:      " 
              << (totalTx > 0 ? 100.0 * totalRx / totalTx : 0.0) << "%" << std::endl;
    std::cout << "  Avg Delay:        " 
              << (delayCount > 0 ? totalDelay / delayCount : 0.0) << " ms" << std::endl;
    std::cout << "  Routing Overhead: " 
              << g_overhead.totalRoutingBytes << " bytes" << std::endl;
    std::cout << "  Flow Count:       " << stats.size() << std::endl;
    std::cout << "================================" << std::endl;

    Simulator::Destroy();
    return 0;
}
