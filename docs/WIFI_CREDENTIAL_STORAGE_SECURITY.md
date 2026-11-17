# Wi-Fi credential storage — security review and safe migration

**Status: review required, no encryption migration or device action performed.**

## Verified source facts
- `esp32/main/main.c` keeps SSID/password in a `Credentials` struct and persists the structure in ESP-IDF NVS with `nvs_set_blob(..., "wifi", ...)`.
- `wifi_init()` uses `esp_wifi_set_storage(WIFI_STORAGE_RAM)`; that applies to the Wi-Fi driver's storage choice and **does not encrypt the separate custom NVS blob**.
- The tracked `esp32/sdkconfig.verified` has `# CONFIG_NVS_ENCRYPTION is not set` and `# CONFIG_FLASH_ENCRYPTION_ENABLED is not set`. It also does not enable secure boot.
- The tracked configuration is build evidence, **not proof** of the actual settings or memory state of an already-flashed ESP32-S2.

## Threat and scope
Someone with physical access to the board or a copied firmware/flash dump may be able to recover a Wi-Fi passphrase if the actual flash contains a plaintext NVS record. This does **not** establish a remote-execution path, network compromise or evidence of a real leak.

## Safe engineering gate (no automatic flash/eFuse changes)
1. **Identify** ESP32-S2 revision, partition table, actual flashed SDK build, NVS/flash encryption settings, and current backup/restore process on a test device. No production keys in test records.
2. **Choose a supported ESP-IDF 5.5 mechanism** for protecting custom NVS data. Review the precise ESP32-S2 SoC capabilities, key provisioning and eFuse semantics against current vendor docs. Encryption and secure-boot provisioning may be one-way; do not burn eFuses without a separate device-specific reviewed procedure.
3. **Design a recoverable migration** for legacy plaintext `wifi` blobs. Handle first boot, incorrect data sizes, power interruption, missing keys and failed writes; do not erase old data before a verified encrypted write and a documented recovery path.
4. **Avoid credential disclosure:** no passwords or Wi-Fi frames in CI, serial logs, crash dumps, Git history, releases, app screenshots or troubleshooting tickets. Zeroize transient password copies where feasible after hardware-compatible verification.
5. **Test on a non-production board:** new provisioning, upgrade from the old format, restart/power loss, reset, reconnect, bad password, restore and cleanup of plaintext remnants. Verify with a controlled flash inspection **without publishing the captured secrets**.
6. **Release gate:** audited config, repeatable build and physical PASS evidence, safe rollback/recovery, then separately authorize production-device migration.

**Interim precautions:** Treat Wi-Fi passwords provisioned on experimental boards as accessible to someone with physical possession. Use a dedicated guest/test network, not a privileged personal network. If a board or dump is lost or shared, change its Wi-Fi passphrase.

Source review alone cannot guarantee confidentiality of stored credentials. Do not enable flash encryption or change NVS layout automatically in CI or during a firmware install.
