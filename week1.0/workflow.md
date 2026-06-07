# OLSR / NS-3 Simulation Workflow

## Objective

Build a reproducible NS-3 simulation workflow for OLSR routing in the project communication subsystem. The simulation supports the Chapter 5 design claim that cluster heads can form an inter-cluster Mesh backbone and use route bypass when direct links to the ground station are blocked.

This week 1.0 version is workflow-only. It defines what to simulate, what to record, and how to judge whether the future simulation is useful.

## Simulation Platform

- Runtime: Ubuntu virtual machine.
- Simulator: NS-3.
- Protocol focus: OLSR.
- Related future comparison: AODV, because Chapter 15 mentions AODV/OLSR simulation comparison.

## Design Mapping

| Project design element | OLSR simulation mapping |
|---|---|
| 30 UAVs in 10 km axis | 30 mobile wireless nodes in a 10 km corridor |
| Clustered network | 3-5 logical groups, each with one cluster-head candidate |
| Inter-cluster Mesh | OLSR-enabled ad hoc network among cluster heads and relay-capable nodes |
| Ground base station | One fixed sink/control node at one end or center of corridor |
| Relay nodes | Optional high-position relay nodes introduced in scenario variants |
| Link blind zone | Time-windowed link degradation, loss increase, range reduction, or node/path removal |
| Evidence for Chapter 15 | Repeatable NS-3 configs, logs, FlowMonitor data, metrics, and result interpretation |

## Baseline Run Definition

The baseline should be intentionally simple:

1. Place 30 UAV nodes along a 10 km operation corridor.
2. Mark 3-5 nodes as cluster-head candidates for analysis.
3. Add one ground station.
4. Use NS-3 OLSR routing.
5. Generate UDP traffic from cluster-head candidates to the ground station.
6. Generate low-rate background status traffic from ordinary UAV nodes if needed.
7. Run for 300 s.
8. Collect FlowMonitor output, route snapshots, and summary CSV.

Do not add every real-world effect in the first run. The first run proves that the scenario, metric extraction, and OLSR route formation are working.

## Run Matrix

| Run group | Scenario | Main question | Required output |
|---|---|---|---|
| G0 | Static baseline | Does OLSR form usable multi-hop routes? | PDR, delay, route table snapshot, overhead |
| G1 | Low-speed mobility | Are routes stable under 5-15 m/s motion? | PDR over time, delay, route changes |
| G2 | Relay-assisted | Do relay nodes reduce hop count or improve PDR? | Hop count, PDR, delay, overhead |
| G3 | Blind-zone event | How long does OLSR take to recover after link degradation? | Recovery time, drop burst length, route changes |
| G4 | Load scaling | Does OLSR overhead remain acceptable under more status traffic? | Routing overhead ratio, throughput, PDR |

For any random mobility or random placement, run at least 5 seeds and report mean plus worst case.

## Metrics

Primary metrics:

- Packet delivery ratio.
- Mean and P95 end-to-end delay.
- Jitter.
- Throughput.
- Route recovery time after link degradation.
- Hop count to ground station.

Routing overhead metrics:

- OLSR HELLO count and bytes.
- OLSR TC count and bytes.
- Total routing bytes.
- Routing overhead ratio: routing bytes / application payload bytes.

Diagnostic metrics:

- Packet drops by time window.
- Route table changes.
- Number of reachable nodes over time.
- FlowMonitor lost packet count.

## Workflow Steps

### Step 1: Environment Record

Inside Ubuntu VM, record:

- Ubuntu version.
- NS-3 version.
- NS-3 source path.
- Build profile.
- Compiler version.
- Whether OLSR examples or module tests can run.

Save this to `notes/environment.md`.

### Step 2: Parameter Freeze

Before implementing the simulation, fill `notes/scenario-parameters.md` with:

- Node count and node roles.
- Corridor geometry.
- Wireless model.
- Mobility model.
- Traffic model.
- Simulation duration.
- Seed count.
- OLSR default or modified parameters.

### Step 3: Minimal Scenario Implementation Later

Later code should be written only after this workflow is accepted. It should create the smallest NS-3 script that can produce:

- Route formation.
- UDP delivery.
- FlowMonitor metrics.
- A saved run config.
- A summary CSV.

### Step 4: Sanity Validation

Before trusting any results:

- Confirm route tables are not empty after convergence.
- Confirm at least one route is multi-hop.
- Confirm the ground station receives traffic from every expected source.
- Confirm FlowMonitor flow counts match expected traffic flows.
- Confirm results are repeatable for the same seed.

### Step 5: Variant Runs

Run variants one at a time. Change only one major factor per run group:

- Mobility.
- Relay count/placement.
- Link degradation event.
- Traffic load.

Record each run in `notes/run-matrix.md`.

### Step 6: Result Interpretation

Use this interpretation template:

1. OLSR connectivity:
   - Did all expected nodes have routes?
   - Were routes multi-hop where expected?
2. Delivery quality:
   - Was PDR acceptable?
   - Did delay remain reasonable for status/control-like traffic?
3. Recovery:
   - After a degraded link, how long was delivery disrupted?
   - Did OLSR find an alternate path?
4. Overhead:
   - How much traffic did OLSR control messaging consume?
   - Is the overhead acceptable for the planned data/control link assumptions?
5. Project relevance:
   - Which Chapter 5 Mesh claims are supported?
   - What still requires AODV comparison or field validation?

## Output Artifacts

Expected future artifacts:

```text
results/
  olsr_<scenario>_<seed>_flowmon.xml
  olsr_<scenario>_<seed>_summary.csv
  olsr_<scenario>_<seed>_routes.log
  olsr_<scenario>_<seed>_config.json

figures/
  olsr_pdr_delay.png
  olsr_overhead.png
  olsr_recovery_time.png
```

## Week 1.0 Done Definition

Week 1.0 is complete when:

- This workflow exists.
- The folder structure exists.
- AGENTS.md records that OLSR simulation uses NS-3 in Ubuntu VM.
- No simulation code has been written yet.

