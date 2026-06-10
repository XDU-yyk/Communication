# OLSR 仿真提交说明（week1.1）

## 1. 提交内容说明

本文件夹用于提交本周 OLSR 路由仿真的第一版运行结果，主要包含两部分：

```text
olsr-sim-submit-1.1/
├── figures/
│   └── olsr静态场景基线仿真结果.png
└── olsr-week1.1-run/
    ├── artifacts.txt
    ├── olsr-sim.cc
    ├── olsr_flowmon.xml
    ├── olsr_routes.log
    ├── olsr_summary.csv
    └── run.log
```

其中，`olsr-week1.1-run/` 是从 Ubuntu 虚拟机中导出的仿真运行结果文件夹。由于该文件夹内已经包含本次使用的 `olsr-sim.cc` 源文件，因此本提交包没有再单独建立 `source/` 或 `src/` 文件夹。

## 2. 仿真环境

- 操作环境：Ubuntu 虚拟机
- 仿真平台：NS-3.43
- 仿真协议：NS-3 内置 OLSR 路由协议
- 仿真代码位置：`olsr-week1.1-run/olsr-sim.cc`
- 结果导出目录：`olsr-week1.1-run/`

## 3. 本次仿真场景

本次提交的是 OLSR 静态基线场景，主要用于验证 30 架无人机规模下 OLSR 路由是否能够在 NS-3 中正常跑通并形成多跳转发路径。

主要参数如下：

| 参数 | 数值 |
| --- | --- |
| 场景 | static |
| 无人机数量 | 30 |
| 仿真时长 | 300 s |
| 随机种子 | 1 |
| 无线通信范围 | 2000 m |
| OLSR Hello 间隔 | 2 s |
| OLSR TC 间隔 | 5 s |

## 4. 结果摘要

根据 `olsr_summary.csv` 和 `run.log`，本次仿真结果如下：

| 指标 | 结果 |
| --- | --- |
| 发送应用包总数 | 10 |
| 接收应用包总数 | 10 |
| 总体投递率 | 100% |
| 平均时延 | 10.5 ms |
| 业务流数量 | 5 |
| 物理层总发送字节数 | 1,092,364 bytes |

逐流结果中，5 条业务流的 PDR 均为 100%。`olsr_routes.log` 中可以看到 OLSR 路由表已经生成多跳路径，说明该静态场景下路由连通性已跑通。

## 5. 文件用途

| 文件 | 用途 |
| --- | --- |
| `figures/olsr静态场景基线仿真结果.png` | 中文结果图，可用于汇报或插入文档 |
| `olsr-week1.1-run/olsr-sim.cc` | 本次运行使用的 NS-3 仿真源代码 |
| `olsr-week1.1-run/run.log` | 终端运行日志 |
| `olsr-week1.1-run/olsr_summary.csv` | 仿真统计摘要，包含逐流指标和总体指标 |
| `olsr-week1.1-run/olsr_flowmon.xml` | FlowMonitor 原始统计结果 |
| `olsr-week1.1-run/olsr_routes.log` | OLSR 路由表快照，用于查看多跳路由 |
| `olsr-week1.1-run/artifacts.txt` | 本次运行相关产物列表 |

## 6. 复现方式

如需在 Ubuntu 虚拟机中复现本次仿真，可将 `olsr-sim.cc` 复制到 NS-3 的 `scratch/` 目录后运行：

```bash
cp olsr-week1.1-run/olsr-sim.cc ~/repos/ns-3.43/scratch/olsr-sim.cc
cd ~/repos/ns-3.43
./ns3 build
./ns3 run scratch/olsr-sim
```

运行后，结果会生成在 NS-3 工程目录下的 `results/` 文件夹中。

## 7. 当前完成情况与后续工作

已完成：

- Ubuntu 虚拟机 + NS-3.43 环境下 OLSR 仿真编译运行。
- 30 架无人机静态基线场景跑通。
- 导出 `run.log`、`olsr_summary.csv`、`olsr_flowmon.xml`、`olsr_routes.log` 等结果文件。
- 根据仿真结果整理中文结果图。

暂未完成：

- 多随机种子重复实验。
- 移动场景仿真。
- 弱链路、遮挡、不同通信范围等鲁棒性测试。
- OLSR 与 AODV 等协议的对比实验。
- 更高业务负载下的正式性能评估。

说明：本次结果适合作为 OLSR 仿真已跑通的基线证据，不宜直接作为最终性能结论。后续正式报告前，建议增加业务包数量、多随机种子和移动/弱链路场景，并结合 FlowMonitor 原始结果进一步复核时延统计。
