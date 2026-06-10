# Week 1.0 — OLSR NS-3 仿真

## 概述

本目录包含基于 ns-3 内置 OLSR 模块的无人机自组网仿真代码，用于验证**第5章 5.2.3节 簇间 Mesh 通信**的可行性，并为**第15章**提供可复现的仿真证据。

## 仿真环境

- **平台**：Ubuntu 虚拟机
- **仿真器**：ns-3（内置 OLSR 模块 `src/olsr/`）

## 场景

| 场景 | 描述 | 对应运行组 |
|------|------|-----------|
| `static` (G0) | 30 架 UAV 固定位置，沿 10km 走廊等距部署，5 个簇头候选，验证多跳连通 | G0 |
| `mobility` (G1) | UAV 以 5-15 m/s Waypoint 随机游走，地面站固定，测试路由稳定性 | G1 |

## 运行步骤

```bash
# 1. 将脚本复制到 ns-3 scratch（跟你之前的操作一样）
cp ~/Desktop/olsr-sim.cc ~/repos/ns-3.43/scratch/

# 2. 编译
cd ~/ns-allinone-3.43/ns-3.43
./ns3 build

# 3. 运行 G0 静态基线（30 机，300s）
./ns3 run "scratch/olsr-sim --scenario=static --nUavs=30 --simTime=300 --seed=1"

# 4. 运行 G1 低速移动（5-15 m/s 路点）
./ns3 run "scratch/olsr-sim --scenario=mobility --nUavs=30 --simTime=300 --seed=1"

# 先快速测试（60s 验证路由连通性）
./ns3 run "scratch/olsr-sim --scenario=static --nUavs=30 --simTime=60 --seed=1"
```

## 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--scenario` | static | static (G0) / mobility (G1) |
| `--nUavs` | 30 | 无人机数量 |
| `--simTime` | 300 | 仿真时长（秒） |
| `--seed` | 1 | 随机种子 |
| `--resultsDir` | results | 输出目录 |
| `--wifiRange` | 2000 | WiFi 通信范围（米） |
| `--helloInterval` | 2.0 | OLSR Hello 间隔（秒） |
| `--tcInterval` | 5.0 | OLSR TC 间隔（秒） |

## 输出产物

| 文件 | 说明 |
|------|------|
| `results/olsr_flowmon.xml` | FlowMonitor 原始数据 |
| `results/olsr_summary.csv` | 逐流 + 聚合统计摘要 |
| `results/olsr_routes.log` | 路由表快照（验证多跳连通） |
| `results/olsr_run_config.json` | 完整运行参数配置 |
| `figures/olsr_pdr_*.png` | PDR 直方图 |
| `figures/olsr_delay_*.png` | 延迟图 |
| `figures/olsr_summary_*.png` | 摘要表 |

## 关键设计

- **传播模型**：`RangePropagationLossModel`，范围 2000m → 10km 走廊产生 ~5-6 跳
- **WiFi**：802.11a AdHoc，6Mbps 固定速率
- **流量**：簇头→地面站（1包/10s）+ 其他UAV→地面站（1包/100s）
- **路由**：ns-3 内置 OLSR（默认 Hello=2s, TC=5s）
- **移动（G1）**：WaypointMobilityModel，5-15 m/s，走廊范围内随机路点

## 验证清单

- [ ] 路由表非空（`olsr_routes.log` 中可验证）
- [ ] 至少一条多跳路由存在于 10km 场景
- [ ] 所有簇头到地面站的 Flow 均有非零 PDR
- [ ] 结果对同一种子可重复
- [ ] G0 的 PDR 应高于 G1（移动带来路由震荡）
