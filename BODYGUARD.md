# BODYGUARD Fork

Openpilot fork optimized for Comma Body as a security camera and teleop platform.

## What it does

- Disables all local video recording (no rlog, qlog, hevc, ts files)
- Disables all phone-home services (no registration, upload, telemetry, crash reporting)
- Keeps only the 25 processes needed for WebRTC streaming, body teleoperation, control enable path, and OTA updates
- Fixes V4L hardware encoder contention (7 sessions → 3) that caused frame drops and camera resets
- Adds crash recovery for webrtcd (`restart_if_crash`)
- Disables local audio daemons (`micd`, `soundd`) to avoid device contention with WebRTC
- Preserves normal onroad transitions/UI in local unregistered mode (BODYGUARD bypasses registration startup gate)

## Activation

`BODYGUARD=1` is exported in `launch_env.sh` by default. To restore stock openpilot, comment out or remove:

```bash
# in launch_env.sh
export BODYGUARD=1
```

## Running processes

| Process | Port | Purpose |
|---------|------|---------|
| camerad | — | Camera frame capture |
| stream_encoderd | — | H.264 encoding for WebRTC |
| webrtcd | 5001 | WebRTC stream server |
| webjoystick | 5000 | Teleop web UI (HTTPS) |
| bridge | — | Cereal message bridge |
| card | — | Publishes carParams (`notCar`) used for process gating |
| joystickd | — | Joystick commands → body movement |
| selfdrived | — | Publishes selfdriveState/onroadEvents for control enable path |
| modeld | — | Publishes `modelV2` |
| dmonitoringmodeld | — | Driver monitoring model |
| dmonitoringd | — | Publishes `driverMonitoringState` |
| locationd | — | Publishes `livePose` |
| calibrationd | — | Publishes `liveCalibration` |
| paramsd | — | Publishes `liveParameters` |
| torqued | — | Publishes `liveTorqueParameters` |
| lagd | — | Publishes `liveDelay` |
| radard | — | Publishes `radarState` |
| plannerd | — | Publishes `longitudinalPlan`, `driverAssistance` |
| sensord | — | Sensor feed (`accelerometer`, `gyroscope`) |
| pandad | — | Manager infrastructure |
| hardwared | — | Manager infrastructure |
| logmessaged | — | Log routing |
| timed | — | System time sync |
| updated | — | OTA updates (offroad only) |
| ui | — | On-device display, WiFi config |

## Connecting

**Teleop UI:** `https://<body-ip>:5000` (self-signed cert)

**Custom WebRTC client:** POST to `http://<body-ip>:5001/stream` with:
```json
{
  "sdp": "<offer SDP>",
  "cameras": ["driver", "wideRoad"],
  "bridge_services_in": [],
  "bridge_services_out": []
}
```

## Branch info

- **Branch:** `bodyguard`
- **Based on:** `upstream/release-tici` (openpilot v0.10.0, `551d088497`)

To pull upstream updates:
```bash
git fetch upstream
git merge upstream/release-tici
```

## Upstream compatibility

All changes are guarded by the `BODYGUARD` env var. The upstream process list in `process_config.py` is untouched — a filter block is appended at the bottom. Merging upstream changes should be conflict-free.

### Modified files

- `launch_env.sh` — sets `BODYGUARD=1`
- `system/manager/process_config.py` — appended allowlist filter
- `system/manager/manager.py` — guarded registration/sentry skip
- `system/hardware/hardwared.py` — allows startup in BODYGUARD local/unregistered mode
- `system/sentry.py` — early return in BODYGUARD mode
