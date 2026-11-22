# Encryption migration plan — ESP32-S2 Wi-Fi credentials

**Status: PLAN ONLY.** No firmware was flashed, no eFuse was burned, no NVS was
erased or modified. Every hardware step below needs the owner's explicit,
separate approval. Background and threat model:
[WIFI_CREDENTIAL_STORAGE_SECURITY.md](WIFI_CREDENTIAL_STORAGE_SECURITY.md).

## Current state (verified from source, 2026-11-21)

| Item | Value | Source |
| --- | --- | --- |
| Wi-Fi SSID/password | plaintext `Credentials` blob, NVS namespace `csi_radar`, key `wifi` | `esp32/main/main.c` (`nvs_set_blob`) |
| Wi-Fi driver storage | `WIFI_STORAGE_RAM` (driver does not persist) | `esp32/main/main.c` |
| Flash encryption | not set | `esp32/sdkconfig.verified` |
| NVS encryption | not set | `esp32/sdkconfig.verified` |
| Secure Boot | not set (SoC supports Secure Boot V2 RSA) | `esp32/sdkconfig.verified` |
| Flipper side | SSID/password only sent over UART as hex, never written to SD logs | `flipper_app/csi_radar.c`, `PROTOCOL.md` |

Risk: someone with physical access to the board or a flash dump may read the
Wi-Fi passphrase. No remote path is known.

## Credential exposure by location (source review, 2026-11-22)

| Location | Exposure | Mitigation in source | Status |
| --- | --- | --- | --- |
| RAM — global `credentials` | Needed for reconnects for the life of the firmware; readable by a debugger/JTAG or a RAM dump | inherent while Wi-Fi is configured; JTAG can be disabled by eFuse in option C | accepted, documented |
| RAM — temporary copies (`saved`, parsed command fields, UART lines) | could survive after use | `secure_zero()` volatile wipe (PR #6, CodeQL #1 fixed on main) | PASS (source, CI); device not reflashed |
| RAM — `wifi_config_t` stack copy in `configure_wifi()` | left on the stack after `esp_wifi_set_config` | wiped after use (separate fix PR) | PENDING merge |
| Wi-Fi driver storage | `WIFI_STORAGE_RAM`, driver does not write its own copy to flash | — | PASS (source) |
| NVS `csi_radar/wifi` blob | plaintext in flash | options A/B below | OPEN |
| Flipper SD | password is not written to SD logs or settings | — | PASS (source) |
| UART link Flipper↔ESP | hex-encoded, not encrypted; physical wire only | out of scope for a local wired link | accepted, documented |

**Device note:** the boards in use still run the firmware in `ready/`, which predates
these fixes. Nothing here claims the physical device is fixed.

## Rollback and data recovery requirements

| Step | Requirement before proceeding |
| --- | --- |
| Before any flash | Full flash read-back (`esptool read_flash`) of that board, stored locally and encrypted, never committed; SHA-256 recorded |
| Restore proof | The backup is written back to the **same** board and boots, before any eFuse change |
| Option A rollback | Previous `ready/` binaries re-flash cleanly (no eFuse or partition change); Wi-Fi is re-entered from the Flipper |
| Option B rollback | Only possible while the development-mode reflash budget remains; record `FLASH_CRYPT_CNT` before/after; a lost key means re-flash from plaintext image is impossible |
| Option C rollback | **None.** Release-mode encryption and Secure Boot are permanent; a lost signing key bricks updates |
| User data | Only Wi-Fi credentials and threshold live in ESP NVS; both can be re-entered from the Flipper, so NVS loss is recoverable by re-provisioning. Flipper SD data (`apps_data/csi_radar`) is untouched by ESP work |
| Approval | Owner approves board serial, exact commands and backup hash before each hardware step |

## Options

| Option | Protects | Hardware risk | Reversible | Notes |
| --- | --- | --- | --- | --- |
| **A. Do not persist the password on the ESP** | flash dumps of the ESP | none | yes | Flipper re-sends credentials after each ESP boot; moves plaintext to the Flipper SD instead (see below). Software-only. |
| **B. Flash Encryption (Development mode) + NVS encryption** | flash dumps | medium: burns eFuses, limited reflash count | partially (dev mode allows a few plaintext reflashes) | Needs an `nvs_keys` partition → **partition table change**, existing NVS cannot be kept. |
| **C. Flash Encryption (Release mode) + Secure Boot V2** | flash dumps and unsigned firmware | high: one-way eFuses, bricking possible on mistakes | **no** | Production only, after B is proven on a spare board. |

Recommendation: **A as interim**, then **B on a spare board** as a lab exercise.
**C only after B passes all gates, on one board, with a written key-custody plan.**

### Option A detail (software only)
- ESP keeps credentials in RAM only; on `HELLO` the Flipper sends `WIFI ...` from its
  own settings. Flipper storage would then hold the password: store it in a
  separate file, never in CSV logs, never in screenshots, and document that the
  Flipper SD is not encrypted. Consider "do not remember password" as the default.
- Migration: first boot of the new ESP firmware deletes the legacy `wifi` blob
  **only after** a successful connection with Flipper-provided credentials.
- Tests: host protocol tests, malformed/oversized `WIFI` frames, power loss
  between connect and blob deletion, reconnect after ESP reset.

### Option B detail (one spare board, lab only)
1. Record board revision, eFuse summary (`espefuse.py summary`, read-only) and the
   current partition table. Store the record without any secrets.
2. Back up the full flash image to an encrypted local location; verify it restores
   on the same board **before** any eFuse operation.
3. Add `nvs_keys` partition; enable `CONFIG_FLASH_ENCRYPTION_ENABLED`
   (Development mode) and `CONFIG_NVS_ENCRYPTION`. Build is reproducible and its
   sdkconfig is committed **without keys**.
4. **Approval gate 1:** owner approves the exact command list and board serial.
5. First boot burns the flash-encryption key eFuse and encrypts flash. Re-provision
   Wi-Fi from the Flipper (old NVS is not migrated because the layout changed).
6. Verify: app works, `nvs_get_blob` round-trip, raw flash read shows no plaintext
   SSID/password (check locally, do not publish dumps).
7. Document remaining development-mode reflash budget (`FLASH_CRYPT_CNT`).

### Option C gate (not scheduled)
Release-mode flash encryption and Secure Boot V2 permanently disable plaintext
flashing and require signed images. Prerequisites: Option B PASS, offline signing
key custody (never in Git or CI), documented recovery, and a separate approval.

## Acceptance checklist (all PENDING)

| Check | Status |
| --- | --- |
| Option A implemented with host tests | PENDING |
| Legacy blob removal after verified connection | PENDING |
| Spare board identified and eFuse summary recorded | PENDING |
| Full flash backup restored successfully before eFuse change | PENDING |
| Option B build reproducible, no keys in repo | PENDING |
| Raw flash inspection shows no plaintext credentials | PENDING |
| Owner approval for any eFuse / flash / NVS operation | REQUIRED, not given |

## Hard rules
- No eFuse burning, flash erase, NVS reset or firmware flashing without explicit
  owner approval for that specific board and command list.
- No keys, passwords, flash dumps or eFuse key material in Git, CI, logs, issues
  or releases.
- No PASS without a real, observed device result.
