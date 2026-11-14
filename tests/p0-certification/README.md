# Independent P0 regression suite

Run from an isolated firmware worktree containing this directory:

```sh
python3 tests/p0-certification/run.py \
  --firmware /workspace/opentail-p0 \
  --android /workspace/opentail-android-baseline \
  --output /workspace/p0-evidence
```

Requires C/C++ compilers, Python 3 and Java with `jdk.compiler` (no Gradle,
Android SDK or JUnit required for the cross-language codec check). Both source
trees are explicit. The runner records their SHAs and dirty status, retains logs,
and returns nonzero for any failure or blocked check. Do not run the inherited
host suite concurrently: its executables use fixed `/tmp` names.

## What this adds

- Production preview → pair-store → wire-codec composition with zero-LED boot,
  O1=37 and O2=144, 100 alternating lane heartbeats, a single O1 Save including
  zone name/color order/RGB and white gains/gamma, and independently saved O2.
- Eighteen power-cut boundaries: every phase of the nine-phase store transaction
  for the second output and for a metadata/hardware change. Exact whole-object
  comparisons reject partial metadata persistence and cross-output contamination.
  Phase 9 models an ambiguous response after durable completion; API failure is
  asserted, then recovery discovers the complete durable candidate.
- Actual firmware-encoded 667-byte maps consumed by the pinned Android production
  parser. Checks 37/144 lengths, metadata, upload reassembly, all 1,334 single-byte
  corruptions, truncated maps and wrong-lane readback.
- Runs the existing full firmware suite rather than duplicating its sequence,
  stale revision, lease, CRC, COMMITTING, effects continuity, STOP and physical
  arbitration regressions. Existing tests are supporting evidence, not newly
  independent tests. The composition fixture reuses its fake NVS adapter openly.

Panel v4 persistence uses wire schema byte **3**; these are not contradictory
version numbers. The native pair-store test uses the real storage state machine
with fake durable I/O. Quiet-window promotion is explicitly host-simulated.

## Certification boundary / outstanding tests

This is a host regression suite, **not a complete P0 certification**. It does not
invoke `ble_transport.c` through a real GATT connection or execute Android's
GeometryTransactionManager/UI. The generated snapshot header is synthetic;
wire payloads come from the real firmware codec. It does not prove Android
reports success only after authoritative durable readback.

The exact combined B scenario (37/144 effects running while UI switches and
Save occurs) remains a device integration gate. The full host suite tests real
renderer continuity separately; the new 37/144 case tests configuration isolation.
It must not be described as optical continuity certification.

Required device follow-up on exact flashed firmware/APK identities:

1. Start both lanes at zero. Live-configure 37/144 and distinct continuous effects.
   Capture both outputs against an uninterrupted reference while switching lanes
   100 times. Save O1 with renamed zones, changed color order and supported gains.
   Check authoritative O1 readback; O2 stays active and not durable. Save O2,
   reconnect, and compare both full configurations.
2. Drop connection before/during each upload and after durable completion before
   response. Inject stale revision, wrong CRC, missing/reordered UPDATE and lease
   expiry. Require explicit unresolved/failure state until authoritative readback;
   no automatic success from a write ACK. Reload and retry in a fresh session.
3. During each scenario issue STOP and each physical override. Record output
   frames/latency and verify physical arbitration authority. Repeat randomized
   power cuts on real NVS. Host simulation does not establish electrical safety.

## Recorded run

Firmware `e19b81f60d99c3b43ff93bf6295f8becb14aebe9`, Android
`29fcb1288677ab28bf7081edc0bf0991d8734cc0`: new composition, full firmware host
suite and cross-language codec checks **PASS**. Java compiler entrypoint was
available through its module despite no `javac` executable. No production source
changed. Firmware later advanced to `1dfe28ca50ad54fd92c6a0b5fb1c919cd7687d08`;
that later head is not covered by this recorded run. See `evidence/` for logs.
