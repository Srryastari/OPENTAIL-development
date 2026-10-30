# CORE durable profile recovery v2 — firmware safety contract

**Status:** software candidate only; no road-use, flashing or physical safety acceptance.

## Profiles and edit tokens
Each of `ot_trig_cfg` and `ot_output_cfg` stores CRC-checked `stage_v3` and `active_v3` records. Version 2 embeds `previous`, `candidate`, and previous/candidate `uint16_t` edit revisions (per-output for geometry). Legacy `profile_v1` / `profile_v2` remain read-only. The #46 draft's v1 journals are recognized and conservatively upgraded for reading; their pre-v2 history cannot be reconstructed, so upgrading an existing v1 journal begins at known 1/2 tokens. A successfully committed v2 stage increments the edit token. A failed commit does not. The token never wraps: once `UINT16_MAX` is reached, writes fail closed pending a future versioned transport.

HTTP trigger and output POST require `expected_config_revision` from a fresh GET, or reject; BLE trigger 11/15-byte and existing output packet/UUID interfaces stay byte-identical. Trigger GET exposes `config_revision` (latest durable edit) and `active_config_revision` (rendered); output GET exposes `config_revisions[]` and `active_config_revisions[]`. `revision` remains schema version; `saved_on_device` and BLE persisted remain false while pending. An idempotent output write returns the current state without incrementing a revision or publishing an unjournaled generation.

## Crash-safe application boundary
1. Admission validates a recent token, no outstanding stage, no raw/debounced GPIO, no show/simulation/demo, sampler+promoter readiness and an available save mutex.
2. NVS `stage_v3` is committed **before** the user sees a successful staged write. Active RAM remains `previous`.
3. A low-priority promoter requires a settled GPIO bank and a stable ISR any-edge epoch; it writes and commits `active_v3`, retaining both `previous` and `candidate`.
4. It rechecks GPIO/epoch, erases `stage_v3` and commits/verifies that erase **before** publishing new active RAM.
5. It rechecks GPIO/epoch and swaps RAM plus active-revision token under the CORE state lock. An edge during erase keeps RAM on `previous` and pending on `candidate`. On the next idle attempt, the promoter can use `active_v3` if the stage key is already gone.
6. Boot recovery always chooses `previous` plus pending `candidate` from staged (preferred) or active (if no stage). Thus a reboot *during or after* stage erasure cannot reinterpret the durable candidate as live while an input is held. Revalidation after every reboot is intentionally conservative, even if the previous session had activated candidate.
7. NVS corruption or stage-finalization failure sets a fault; it must not load an invalid candidate or erase the recovery path in response.

**No atomic hardware transaction exists across NVS flash, ISR and two CPU cores.** This approach avoids claiming an impossible single atomic instruction: the durable record retains prior semantics and no candidate becomes live merely because a flash key is removed. Physical GPIO timing/ISR and flash failure injection must validate the residual interleavings before safety acceptance.

## Software tests and physical blockers
- `tests/test_config_journal.c`: 256 GPIO-mask recovery cases, atomic state boundaries, CRC/invalid revision and v1-upgrade behavior.
- `tests/test_nvs_finalization_contract.py`: source verification of ordering, pending replay, persistent tokens and REST admission.
- Full CORE, Effects 120 modes, BLE, Triggers, ESP-IDF 5.5.5, Studio/Android exact-head CI is required; result depends on verified immutable SHA.
- Still unverified: actual NVS torn-write/brownout injection at every commit/erase, GPIO ISR latency and cross-core races, physical asserted-input LED output, dual-MCU failover, full device rollback protection to pre-v3 firmware, optical gamma/timing, and 22 physical release checks.
- An OTA rollback to firmware that ignores v3 journal keys remains a safety-policy blocker; that firmware cannot honor this recovery contract. Do **not** approve flash/release until rollback eligibility is pinned to compatible firmware or an independent boot policy is verified.

Firmware PR #49 is stacked on PR #46/#43; Triggers #48 is an independent negative regression review. No main merge, hardware flash or release evidence waiver is authorized.
