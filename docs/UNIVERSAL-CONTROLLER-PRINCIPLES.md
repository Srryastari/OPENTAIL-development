# Universal controller requirements

These requirements apply to current components and future changes. Adaptability
works both ways: the controller adapts to user lighting and installations may
adapt to supported controller interfaces. Neither implies unverified electrical
compatibility or removal of real resource limits.

- Fresh outputs contain zero configured LEDs, panels and zones. User actions
  define actual counts, ranges, names, topology and functions.
- Never infer an installation from a bench, vehicle, output index, driver buffer
  or test fixture. The approximately 1,245–1,246 LED bench and its 522 LED
  subsections are examples, never factory configurations or capacity policy.
- Driver allocation, board pins, protocol limits and configured geometry are
  separate. Report limits honestly; do not present allocation as user geometry.
- Live authoring and role-independent effects do not require persistence. One
  Save persists; Reload reads. Internal sequencing must stay automatic.
- Outputs retain independent layouts, effect state and live sessions. Preserve
  stable identities, animation continuity, STOP, authorization and safety
  priority in user-assigned zones. Gaps/unassigned zones imply no vehicle role.
- Preserve valid saved data, including deliberately configured one-pixel
  installations. Do not erase NVS or infer that a historical saved profile was
  synthetic merely from its pixel count.
- Simple, nonduplicated UI; no redundant commit prerequisite. Unsupported
  topology must fail explicitly rather than be flattened or fabricated.
- Support growth across compatible lighting applications without promising
  unbounded counts, new drivers or unqualified physical hardware.

## Change review and evidence

Changes affecting defaults, migrations, adapters or telemetry require empty-map
roundtrips, explicit nonempty user fixtures and saved-data preservation tests.
Changes affecting arbitration require tests for assigned versus unassigned
zones, STOP and independent outputs. Preserve the 140-effect source identity
and continuity tests. Scope changes to demonstrated defects; no speculative
hardware redesign. Software tests are not physical acceptance.

Legacy v1 cannot represent every Panel v4 topology. Return unsupported when
conversion is lossy; do not manufacture a one-LED compatibility profile.
Internal effect views with no logical pixels have zero targets even if an RMT
buffer is allocated. Existing historical fixture numbers remain valid in tests
only when those tests explicitly construct them and never install them at boot.
