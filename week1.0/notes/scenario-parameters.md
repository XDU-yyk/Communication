# Scenario Parameters (Freezed)

Finalized parameters for OLSR simulation implementation.

| Parameter | Baseline value | Notes |
|---|---|---|
| Simulator | NS-3 (ns-3.43) | Run inside Ubuntu VM |
| Routing protocol | OLSR | ns-3 built-in (src/olsr/) |
| UAV nodes | 30 | Matches project communication target |
| Ground station nodes | 1 | Sink/control endpoint at corridor origin |
| Relay nodes | 0 in baseline | Add 1-3 in G2 variants (future) |
| Corridor length | 10,000 m | Project operation axis (10 km) |
| Corridor width | ±250 m | 500 m total lateral span |
| UAV altitude | 100 m | Fixed altitude |
| Logical clusters | 5 | Cluster heads at 10%, 27.5%, 45%, 62.5%, 80% of corridor |
| Mobility (G0) | Static (ConstantPositionMobilityModel) | No movement |
| Mobility (G1) | Waypoint 5-15 m/s | Random waypoints within corridor bounds |
| Wireless model | 802.11a AdHoc (6 Mbps fixed) | ConstantRateWifiManager |
| Propagation | RangePropagationLossModel @ 2000m max | Ensures multi-hop |
| Traffic type | UDP status/control-like | CH→GS 1 pkt/10s, UAV→GS 1 pkt/100s |
| Packet size | 256 B (CH), 128 B (UAV) | Control/status messages |
| Simulation duration | 300 s | Baseline |
| Seeds | 5 for random scenarios | Report mean and worst case |
| OLSR HelloInterval | 2.0 s | ns-3 default |
| OLSR TcInterval | 5.0 s | ns-3 default |
| Metrics | PDR, delay, jitter, throughput, routing overhead, hop count, route table snapshots | See workflow |
