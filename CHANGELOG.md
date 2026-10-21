# Changelog

## 1.5 Pro — development / device-test candidate

Wi-Fi scan/select, masked ASCII editor including SPACE, bilingual help, persistent
profiles/mute/language, noise/rate-aware auto calibration, handshake/recovery,
queued SD CSV sessions/rotation and standard-Python RAW diagnostic replay.
FAP and ESP firmware both change. Local builds/tests pass; real hardware/room
acceptance gates remain open. No v1.5 stable tag has been created.


## v1.0.0
- Add a native 10×10 monochrome radar launcher icon.
- Set the Flipper application version to 1.0 and add author metadata.
- Publish source, license, build instructions, firmware compatibility and evidence scope.
- Preserve the owner-tested 0.2 sensing, UART protocol and ESP32 binaries unchanged.

Stable scope: E11 / API 87.1 with the official ESP32-S2 Wi-Fi Devboard.
Other configurations are not physically certified. The icon-bearing app is now reported working by the owner; no additional
endurance/recovery validation is implied.

## 0.2
Initial device-tested motion detector, calibration, vibration, framed serial protocol,
ESP32 Wi-Fi setup/persistence and USB capture support.

