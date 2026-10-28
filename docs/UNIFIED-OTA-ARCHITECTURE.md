# OpenTail Unified OTA — implementation contract (draft)

Status: architecture specification only. No OTA endpoints, signing, flashing, migration, or rollback are implemented or hardware-tested by this document.

## Product behavior

The final customer-facing Settings screen has one **Update OpenTail** action. A single authenticated, signed release bundle identifies compatible CORE firmware, Studio UI, configuration schema, and optional migrations. The device validates the complete release before activation. Progress includes Receiving, Verifying, Staged, Restart required, Confirming, Complete, and Rolled back. Never claim completion before post-boot confirmation.

Development tooling may stage Studio independently, but customer release activation must preserve a compatible firmware/Studio pair. No automatic updates while vehicle lighting is actively in use; request a safe maintenance window. Upload/staging may occur without changing active lighting, subject to measured CPU/memory timing and watchdog tests.

## Current baseline and blockers

- `core-firmware/sdkconfig.defaults` selects `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` (no dual OTA slots).
- `core-firmware/main/CMakeLists.txt` embeds `assets/studio.htm.gz` in the application image; `/studio` serves that embedded asset. There is no independent Studio update path.
- Existing output, trigger, and BLE owner information is persisted in NVS. Partition migration could destroy these values.
- The actual firmware binary size, flash partition usage, NVS layout and spare RAM have not been measured here. Do not choose partition offsets or assert that two firmware slots fit until measured.
- Current bench controller cannot receive this update OTA. Initial migration requires a wired flash/recovery procedure and verified settings backup.

## Release bundle contract (v1)

A `.otupdate` bundle contains:
- Canonical signed manifest with format_version, release_id, hardware_targets, minimum_bootloader, firmware_version, studio_version, configuration_schema_version, and per-component SHA-256 + size.
- CORE application image for the specified hardware and partition scheme.
- Studio compressed asset, separate from the application image in the future OTA-capable build.
- Optional versioned migration instructions implemented as reviewed device code (never execute arbitrary code from a bundle).

Authenticity: pin a trusted Ed25519 verification public key in the bootstrapped device; verify the signature over canonical manifest bytes and SHA-256 for every component. Define signing-key rotation and revocation before production. Reject unsigned, malformed, oversized, incompatible, replayed/downgrade (unless explicit authorized recovery), or tampered bundles. TLS is defense in depth, not a substitute for signature verification.

## Storage and atomic activation

1. First establish a partition table with `otadata`, two adequately sized OTA app slots, an immutable recovery route if feasible, NVS partitions, and two Studio asset slots or another A/B storage design.
2. Preflight checks: board identity, flash size, available storage, free RAM, power stability, active vehicle state, release compatibility and signing chain.
3. Authenticate operator, rate-limit upload, stream to inactive slots, hash during write, then verify all components. Never overwrite currently active firmware or Studio assets.
4. Persist a staged transaction record (release ID, firmware slot, Studio slot, schema version) with integrity checking.
5. Enter safe maintenance state; atomically select the pending firmware and matching Studio asset for next boot. Confirm post-boot only after CORE initialization, settings migration, Studio route health, and watchdog checks succeed.
6. On failed boot/health confirmation, roll back firmware **and Studio** to the previous matched pair. Preserve a recoverable copy of pre-migration settings; rollback must restore compatible settings schema.
7. Confirm success only after the running firmware reports the expected release ID and matching Studio version.

A/B firmware alone is not a transactional unified update; Studio assets and schema must roll back as part of the same release.

## Required security and reliability checks

- Upload requires authenticated owner/admin privileges; the AP password alone is insufficient authorization.
- Use constant-memory streaming, bounded uploads and timeouts; avoid blocking real-time trigger arbitration and status processing.
- Test interruption at every stage (power loss, disconnect, bad signature, full flash, hash mismatch, wrong hardware, schema migration failure, watchdog reset, repeated rollback).
- Ensure NVS owner keys, output profiles, trigger profiles, zone assignments and user presets survive success and rollback.
- Verify startup, brake, park, left/right turn, reverse and simultaneous trigger behavior during staging; do not assert automotive fail-safe certification.
- Avoid erasing flash during normal updates. Validate recovery with physical hardware before exposing the feature to customers.

## Development sequence / gates

**Gate 0 — inventory and backup:** Measure current `idf.py size`, actual partition table, firmware image size and NVS namespaces. Implement and test export/import of non-secret user configuration and a secure preservation plan for credentials. Confirm reliable USB recovery. No partition changes yet.

**Gate 1 — migration:** Choose partition sizes from measured artifacts, add dual OTA firmware and Studio A/B storage, preserve NVS, and test wired bootstrap/rollback on a spare ESP32-S3 before the primary bench unit.

**Gate 2 — staging:** Implement authenticated chunked upload and signed manifest verification; implement inactive-slot writes, progress and cancellation before activation.

**Gate 3 — atomic activation:** Implement release transaction state, post-boot health confirmation, matching Studio activation, settings migrations, automatic rollback, and fault injection tests.

**Gate 4 — Studio UI:** Add a single Settings > Update OpenTail surface using approved existing styles, with progress, release details and explicit user confirmation. Do not add separate customer-facing firmware and Studio update buttons.

**Gate 5 — validation:** Two controller bench units, power-loss matrix, preserved profiles, rollback, security negative tests and no unverified claims. Only then consider release.

## Immediate relation to PR #16

PR #16 changes `web-studio/opentail.htm` only; it is **not deployed**. Keep the Output Manager fix separate from the OTA architecture. Do not merge or flash OTA migration as a shortcut to delivering that UI fix.
