# Findings: OLSR NS-3 Simulation Design Decisions

## ns-3 OLSR Module
- ns-3 内置 OLSR 模块位于 `src/olsr/`
- Helper 类：`OlsrHelper`
- 主要属性：`HelloInterval`（默认 2s）、`TcInterval`（默认 5s）、`Willingness`
- 路由计算基于 RFC 3626 规范的 MPR（MultiPoint Relay）优化

## 传播模型选择
- `RangePropagationLossModel`：清晰的控制范围，便于验证多跳行为
- 范围设 2000m → 30 节点 × 10km → 约 5-6 跳
- 参考 tdma-sim 中用的是 CSMA 模拟广播，OLSR 场景需要用真实无线

## WiFi 配置
- `WifiHelper` + `YansWifiPhyHelper` + `YansWifiChannelHelper`
- MAC 类型：`AdhocWifiMac`
- 速率控制：`ConstantRateWifiManager`（减少 PHY 层变量干扰路由行为）

## 移动模型
- G0：`ConstantPositionMobilityModel`（固定位置）
- G1：`WaypointMobilityModel`（5-15 m/s 随机路点）
- 地面站固定

## 路由开销统计
- 通过 OLSR 模块的 TracedCallback 或解析 FlowMonitor 统计
- 本方案使用 FlowMonitor + 自定义统计来获取 HELLO/TC 报文数
