# PR #127: phone-only owner authorization recovery

This work follows the owner's revised requirement: **phone and controller only;
no controller buttons, USB recovery procedure, GPIO changes or ownership-reset
command**. PR #127 stays draft and unmerged. No device flash or erase was run.

## Findings

Starting GitHub HEAD was refreshed to
`dfe1a48c3f48dddf7f4c511f063d3743298ccd6a` before edits.

At that HEAD, `owner_enroll_read()` returns ATT **2 / READ_NOT_PERMITTED** after
an encrypted, bonded link passes `owner_link()`, if the enrollment policy rejects:

- ownership storage fault;
- a legacy `ot_blepair/owner` key without a bound peer (migration requires HMAC);
- a different saved/current identity without authenticated configured-PIN proof.

A link lacking encryption/bonding returns ATT **15**, not 2. Commit failures
return ATT **14**. Android connection and service discovery alone do not establish
owner authorization. Reinstalling the app deletes its encrypted owner key but
normally retains the Android system Bluetooth bond.

ESP-IDF **v5.5.5** pins esp-nimble commit
`685675c0128deafdd201c9eb82e61d227364646c`. Its
`ble_hs_conn_addrs()` normalizes public/random identity aliases 2/3 to 0/1 and
`ble_sm_persist_keys()` updates the connection identity after key distribution.
The original code already used `peer_id_addr`, not the rotating OTA address.
Therefore alias normalization is defensive coverage, **not proof of this
physical failure's cause**. No physical trace or device NVS inspection establishes
which denial branch occurred or proves that the installed image is the stated
GitHub HEAD. Older images have a different BOOT-gated enrollment path; the
current INFO capability includes `owner-bond-v3`.

One reproducible binding defect exists: valid HMAC proof previously bound only
legacy records. If a key-holding owner authenticated under a changed identity,
the stale saved identity remained. A subsequent app reinstall could then fail
enrollment. The correction updates that binding only after valid HMAC proof,
retains the PIN/key, and performs no write for an unchanged identity. This repairs
future recovery for key-proven identity changes; it cannot retroactively prove
a previously unrecorded identity after its last key has been lost.

## Security and persistence

- Same bonded identity can retrieve the existing key and complete normal HMAC
  authentication; no temporary unauthenticated enrollment window is opened.
- A configured PIN continues to admit an SMP-authenticated phone as before.
- Valid identity aliases compare equally; different bytes, public/random
  identity classes, and invalid address types do not.
- HMAC challenges are single-use and now expire after **30 seconds**. Reading a
  new challenge invalidates the previous one. Five failed HMAC attempts retain
  the existing per-connection lockout. Disconnect/reset clears challenge validity.
- Keys, PIN values and peer addresses are absent from the new diagnostic logs.
- No owner credential, bond record or NVS namespace is erased. Ordinary recovery
  of the app key is read-only. Key-proven identity migration writes only the
  existing CRC-protected `ot_blepair/owner_v3` record through its existing
  commit/readback path. Lighting and all other namespaces are untouched.
- No controller button, serial recovery task, remote reset command, Android
  change, renderer change or GPIO assignment is introduced.

If the owner key is lost **and** the retained owner identity cannot be proven by
its surviving bond **and** no configured PIN can be proven, these constraints
provide no secure way to distinguish the owner from another phone. That case
remains blocked; neither deleting NVS nor allowing the next nearby phone to claim
ownership is a secure repair.

## Phone-only procedure (requires the corrected firmware to be installed separately)

1. Keep Android's existing Bluetooth bond. Do not select Forget/Unpair and do
   not clear Bluetooth system data. Do not reset controller settings.
2. Connect the reinstalled app to the existing controller and use its existing
   owner-enrollment/authorization action. No controller button is involved.
3. The encrypted bonded owner identity retrieves the existing app key. Complete
   the fresh challenge/HMAC exchange within 30 seconds; retrying authorization
   requests a new challenge. With a configured PIN, complete Android's pairing
   prompt using that PIN if requested.
4. Require the actual command response **`OK AUTH`** before testing Pixel Probe.
   Then verify that the app enables the capability and CORE accepts the probe.
5. If enrollment still returns GATT 2, the diagnostic distinguishes storage
   fault, legacy key-only ownership, and identity mismatch. Stop at that denial;
   do not forget bonds or factory-reset to make the test pass.

An Android APK that still explicitly demands BOOT is not the phone-only client
flow. This firmware change does not alter Android UI or claim an APK test.

## Verification boundaries

The production C enrollment/load/commit/challenge/verification callbacks are
executed against simulated GAP/NVS; OpenSSL supplies real HMAC-SHA256 to the
host adapter for the mbedTLS calls. Host coverage includes 128 policy combinations,
all 65,536 address-type pairs, lost-app-key retrieval, 100 write-free simulated
reinstall/reboot cycles with byte-identical persisted ownership, authenticated
identity rebind with PIN preservation, legacy migration, invalid proof, replay,
30-second expiry, five-failure lockout, corrupt records and four NVS fault points.
The fake NVS adapter rejects access outside `ot_blepair` and writes outside
`owner_v3`. It does not simulate an RF link or flash wear/atomicity.

The complete CORE host suite additionally checks Panel/Zone journals, palette,
effects, probe policy, Brake/Turn/Reverse arbitration and STOP behavior. The
ESP-IDF 5.5.5 CI build is the embedded compilation gate, not a physical test.

Still required on the existing phone/controller: identify the installed firmware,
observe the exact enrollment branch, confirm surviving Android bond identity,
confirm `OK AUTH`, Android Pixel Probe availability, preserved real lighting
configuration across power cycles, optional-PIN pairing, a different-phone denial,
and physical LEDs/vehicle inputs. No physical PASS is claimed.
