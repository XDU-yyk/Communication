# Run Matrix

Record of simulation runs. Update after each execution in Ubuntu VM.

| Group | Scenario | Seeds | Main variable | Status | Command |
|---|---|---|---|---|---|
| G0 | Static corridor | 1 (first), 5 (full) | None | **Ready** | `./ns3 run "scratch/olsr-sim --scenario=static --nUavs=30 --simTime=300 --seed=1"` |
| G1 | Low-speed patrol mobility | 5 | Mobility 5-15 m/s Waypoint | **Ready** | `./ns3 run "scratch/olsr-sim --scenario=mobility --nUavs=30 --simTime=300 --seed=1"` |
| G2 | Relay-assisted topology | 5 | Relay count and placement | Planned | TBD |
| G3 | Blind-zone event | 5 | Link degradation time/window/location | Planned | TBD |
| G4 | Load scaling | 5 | Traffic rate and packet size | Planned | TBD |

## Run Log

| Date | Group | Seed | Output file | PDR | Avg Delay | Notes |
|---|---|---|---|---|---|---|
| — | — | — | — | — | — | Pending run in Ubuntu VM |
