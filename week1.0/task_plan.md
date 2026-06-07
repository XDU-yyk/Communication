# Task Plan: OLSR NS-3 Simulation (Week 1.0 Implementation)

## Objective
根据 week1.0/workflow.md 完成 OLSR 仿真代码——利用 ns-3 内置 OLSR 模块，实现 30 架无人机在 10km 走廊内的自组网仿真。

## Phases

### Phase 1: Planning & Design [✅]
- [x] 读取 workflow.md，提取场景参数和指标要求
- [x] 读取现有 notes/ 和 tdma-sim 代码参考
- [x] 创建 task_plan.md / findings.md / progress.md

### Phase 2: 仿真脚本 olsr-sim.cc [✅]
- [x] 节点创建（30 UAV + 1 地面站）
- [x] 走廊节点部署（10km x ±250m）
- [x] WiFi AdHoc + RangePropagationLossModel（~2km 范围 → 多跳）
- [x] OLSR 路由安装
- [x] 流量模型（簇头→地面站 + 背景流量）
- [x] FlowMonitor 采集
- [x] G0 静态基线
- [x] G1 低速移动（5-15 m/s WaypointMobility）
- [x] 命令行参数：--scenario=static|mobility, --nUavs, --simTime, --seed
- [x] 输出：FlowMonitor XML + 摘要 CSV + 路由表快照 + 配置 JSON

### Phase 3: 辅助脚本 [✅]
- [x] 创建 run.sh（Ubuntu 编译运行）
- [x] 创建 plot_results.py（可视化）
- [x] 创建 README.md（文档）

### Phase 4: 参数文件更新 [✅]
- [x] 更新 notes/scenario-parameters.md（冻结参数）
- [x] 更新 notes/run-matrix.md（运行矩阵）
- [x] 创建 results/ 和 figures/ 目录占位

## Decisions Log
- 协议：ns-3 内置 OLSR（src/olsr/）
- 传播模型：RangePropagationLossModel（最大范围 2000m）→ 清晰多跳
- WiFi 管理：ConstantRateWifiManager（固定速率，减少 PHY 层变量）
- 节点间距：~333m → 10km 约 5-6 跳
- 簇头：5 个（0, 2.5, 5, 7.5, 10km 处）
- G1 移动：WaypointMobilityModel，地面站固定
