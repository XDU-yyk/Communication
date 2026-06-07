# AGENTS.md

## Project Context

- Workspace: `D:\communication`
- Main source document: `复杂应急救援现场无人机飞行安全控制系统研制策划v2(1).docx`
- This project is about a complex emergency-rescue UAV flight safety control system.
- The core scenario is operation under "断路、断网、断电" conditions, supporting at least 30 heterogeneous UAVs over a 10 km operation axis.
- The user is responsible for the communication subsystem. When working in this repo, prioritize Chapter 5 and Chapter 15 of the main `.docx`.
- Extracted reading artifacts and a longer communication summary are in `.planning/communication_doc_review/`.

## Simulation Environment

- The user's communication simulations are expected to run inside an Ubuntu virtual machine, not directly in this Windows workspace.
- NS-3 is the primary simulation library/environment for communication work, especially TDMA scheduling, collision-free access, delay statistics, routing, and link-behavior validation.
- OLSR routing simulations must also use NS-3 in the Ubuntu VM. Do not replace NS-3 with an ad hoc Python/networkx-only simulation when the user asks for OLSR project simulation work.
- When preparing commands, README steps, build notes, or troubleshooting guidance for simulation, assume an Ubuntu shell and NS-3 workflow unless the user says otherwise.
- This Windows workspace may contain source files, notes, and artifacts, but runnable simulation instructions should be written for the Ubuntu VM/NS-3 setup.

## Communication Subsystem Priorities

- Communication is part of the flight-safety control loop, not just a radio equipment list.
- The subsystem must carry control commands, telemetry, perception data, video/image transmission, logs, and heartbeat/status messages.
- Main communication targets:
  - 10 km communication link.
  - Key air-ground control command end-to-end latency <= 50 ms.
  - 30-UAV state synchronization and coordinated operation.
  - 99.9% safety/scheduling-loop target.
  - 120 h continuous通导监 operation.

## Chapter 5 Design Baseline

- Use a layered heterogeneous ad hoc network:
  - Cluster-based organization for 30 UAVs.
  - Intra-cluster star topology.
  - Inter-cluster Mesh topology.
  - Ground base station and relay nodes for access, blind-zone coverage, and forwarding.
- Link isolation is a safety baseline:
  - Control link and data/video links must be isolated by frequency band, RF front end, PHY/MAC stack, and management plane.
  - Never let video, perception traffic, or COFDM congestion affect the control link.
- Control link baseline:
  - 1.4 GHz narrowband TDMA + GFSK.
  - TDMA frame period: 30 ms.
  - 36 slots per frame: 30 UAV uplink slots, 4 base-station downlink slots, 2 guard slots.
  - Slot duration: about 750 us.
  - Guard interval: >= 50 us.
  - Max payload: 256 B.
  - Symbol rate: about 200 ksps.
- Time synchronization:
  - Ground station uses GNSS timing.
  - Beacon is sent every 300 ms.
  - Target sync precision: base station to cluster head about +/-200 ns, cluster head to member about +/-500 ns, total network clock offset <= +/-1 us.
- Command priorities:
  - P0: emergency avoidance / `emergency_stop`; bypass normal queue and occupy the next available downlink slot.
  - P1: return home / no-fly-zone warning / safety trigger.
  - P2: trajectory, velocity, altitude, hover/wait commands.
  - P3: airspace configuration.
  - P4: heartbeat and maintenance.
- 50 ms latency budget:
  - Ground processing: 2 ms.
  - TDMA queueing: 30 ms.
  - Transmission/relay: 10 ms.
  - Airborne reception: 8 ms.
  - Verification must measure each segment with packet capture/log timestamps, including mean, max, and jitter.

## Data And Video Link Baseline

- Data/video link baseline:
  - 2.4 GHz COFDM/OFDM.
  - 64 subcarriers, 20 MHz bandwidth.
  - AMC supports BPSK, QPSK, 16QAM, and 64QAM.
  - Typical 10 km LOS/near-LOS target rate is about 10-20 Mbps.
- Telemetry:
  - Use compact fixed binary encoding, around 70 B per packet.
  - 30 UAVs at 10 Hz is about 168 kbps.
  - Telemetry packets should not be voluntarily dropped; reduce frequency only as a last step.
- Perception data:
  - Use mixed delta/full update: 9 delta frames + 1 full frame.
  - Under congestion, reduce perception update frequency before sacrificing telemetry.
- Video/image:
  - H.265 1080p@30fps, target around 2 Mbps, low-latency mode.
  - Snapshot mode: 12 MP JPEG, about 500 KB to 1.5 MB, expected return latency about 2-5 s.
  - Reduce bitrate, switch to 720p, send key frames only, or pause video when links degrade.
- QoS lanes:
  - Control lane: independent TDMA link.
  - Telemetry lane: about 5% guaranteed bandwidth.
  - Perception lane: about 15% guaranteed bandwidth.
  - Video lane: best effort.
  - Log lane: no guaranteed bandwidth; cache and backfill after recovery.

## Reliability And Fault Handling

- Link quality monitoring runs every 100 ms and records RSSI, SNR, BER, PER, ARQ retry count, AMC mode, queue depth, throughput, and command latency.
- Health states are green, yellow, orange, and red.
- Control commands use stop-and-wait ARQ with up to 3 retransmissions.
- Telemetry and perception data do not use per-packet ACK.
- Video uses about 5% FEC.
- Logs use TCP/background backfill.
- Heartbeat is 1 Hz. If no heartbeat is received for 3 s, treat the node as offline, defaulting first to a recoverable communication blind-zone assumption.
- Cluster-head failover:
  - Backup cluster head mirrors TDMA schedule and latest member state.
  - After 3 s primary cluster-head loss, backup takes over.
  - Target takeover time is <= 300 ms and scheduling should resume by the next TDMA frame.
- Communication abnormal handling:
  - Yellow: degrade data/video only.
  - Orange: enable control-link retransmission and pause non-emergency new task assignment.
  - Red single-node loss: switch affected UAV to edge autonomy; recover within 30 s if link returns, otherwise return if low battery.
  - Whole-network loss: hover for 30 s, then follow preloaded return route if communication does not recover.
- Important boundary:
  - Distributed soft bus is for device discovery, state synchronization, and upper-layer coordination.
  - It does not provide the 10 km physical wireless link. The 10 km link depends on TDMA/GFSK, COFDM/OFDM, dedicated radios, relays, antenna/link budgets, and field validation.

## Chapter 15 Deliverables And Evidence

- Existing communication basis mentioned in the document:
  - NS-3 TDMA scheduling simulation.
  - GNU Radio / USRP B210 COFDM prototype.
  - AODV/OLSR ad hoc routing simulation comparison.
- Current basis claims:
  - NS-3 validates 30-UAV collision-free access and MAC-layer latency below 50 ms.
  - 1.4 GHz TDMA link budget assumes 500 mW PA, 3 dBi antenna, 10 km free-space loss about 102 dB, receiver sensitivity about -110 dBm, and about 6 dB link margin.
  - The 50 ms end-to-end latency still needs real validation including application framing/deframing and ROS 2 serialization overhead.
- Communication evidence package should include:
  - NS-3 simulation model and scripts.
  - GNU Radio COFDM flow graph and AMC state machine.
  - Link budget spreadsheet.
  - Reproducible test scripts, logs, result plots, and README.
- Direct communication deliverables include:
  - Communication preliminary scheme / three-link architecture, based on Chapter 5.
  - Partial code and source-code list.
  - Detailed design report.
  - User manual section for communication base-station deployment.
  - Communication / 通导监 third-party test reports.
  - Final demo, trajectory analysis, and distributed conflict-resolution statistics.

## Near-Term Work For Communication Tasks

- Check `tdma-sim` in the Ubuntu VM/NS-3 environment against Chapter 5 parameters: 30 ms frame, 36 slots, 30 uplink, 4 downlink, 2 guard, 750 us slot, 50 us guard interval, P0-P4 priority, and 50 ms latency statistics.
- For week 1.0, OLSR work is workflow-only: create the NS-3/Ubuntu VM simulation workflow, scenario matrix, metrics, and evidence plan, without writing simulation code.
- Build or verify the 10 km link budget for both 1.4 GHz control and 2.4 GHz COFDM links, including TX power, antenna gain, receiver sensitivity, feeder loss, fading margin, and obstruction margin.
- Define an end-to-end latency test plan with timestamps at ground app, MAC enqueue/dequeue, PHY TX, PHY RX, airborne decode, and flight-control interface arrival.
- Define the link-monitoring log schema with 100 ms samples for SNR, RSSI, BER, PER, ARQ, AMC, QoS queue depth, throughput, and command latency.
- Prepare the Chapter 15 evidence package so reviewers can reproduce the communication claims.
- Treat the 6 dB link margin as tight: real ground reflection, multipath, and obstruction may consume 3-5 dB. Keep closure options ready, including higher antenna gain, more relay nodes, lower AMC mode, and lower video bitrate.

## Notes

- 记住AGENTS.md
