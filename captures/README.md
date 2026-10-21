# Real capture gate
No real RF captures have been supplied for Pro 1.5. Do not call the synthetic
host fixtures real-room data, and do not mark the 1.5 accuracy gate passed.

Record two private datasets using `tools/csi_usb.py ... record ... --raw`:
30 minutes stationary and a labeled 10-walk session at a fixed placement/threshold.
Store environment, placement, timing, threshold and operator labels alongside them.
Use `tools/replay.py still.log moving.log --output-dir comparison` for diagnostic
CSV/summary output. RAW is decimated to <=10 Hz; offline replay is not bit-exact
with the 20 Hz live engine. Do not publish captures with private metadata by default.
