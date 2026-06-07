# Progress Log

## Session 1 — Implementation Complete
- [x] 读取 workflow.md、notes/ 和 tdma-sim 代码
- [x] 创建规划文件（task_plan.md, findings.md, progress.md）
- [x] 编写 olsr-sim.cc — 核心仿真脚本
  - 31 节点（30 UAV + 1 地面站）
  - 10km 走廊部署，5 簇头等距分布
  - RangePropagationLossModel @ 2000m → 多跳
  - OLSR 路由（ns-3 built-in）
  - G0 静态 + G1 Waypoint 5-15 m/s 移动
  - FlowMonitor + 路由表快照 + 配置导出
- [x] 编写 run.sh — Ubuntu VM 一键运行
- [x] 编写 plot_results.py — PDR/延迟/摘要可视化
- [x] 编写 README.md — 构建/运行/验证说明
- [x] 更新 notes/scenario-parameters.md（冻结参数）
- [x] 更新 notes/run-matrix.md（运行矩阵）
- [x] 创建 results/ 和 figures/ 目录占位

## 待办（Ubuntu VM 中执行）
- [ ] cp olsr-sim.cc → ns-3 scratch/
- [ ] ./ns3 build
- [ ] ./ns3 run "scratch/olsr-sim --scenario=static --nUavs=30 --simTime=300 --seed=1"
- [ ] ./ns3 run "scratch/olsr-sim --scenario=mobility --nUavs=30 --simTime=300 --seed=1"
- [ ] python3 plot_results.py --results-dir results --output-dir figures
- [ ] 验证路由表中有多跳路由
- [ ] 填写 environment.md（Ubuntu/ns-3 版本）
