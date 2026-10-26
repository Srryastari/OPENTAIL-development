# LOCKED — standalone CORE effects and particle architecture (2026-10-26)

This supersedes any proposal to run WLED over Wi-Fi, require a WLED controller/firmware, or expose "Particle Effects" as a separate library.

## Product behavior

- OpenTail CORE (ESP32-S3) owns **one** effect registry with stable WLED-compatible IDs and capability metadata. An effect appears **once** by its name (Solid, Wipe, Fireworks, etc.) regardless of algorithm backend.
- Effects are compiled/ported into CORE. A WLED connection, remote WLED JSON API, Wi-Fi AP, mobile application, or network streaming is **not** required to render frames.
- Particle physics is a **private rendering subsystem** of that same engine. Particle emitters, life, trails, collisions and motion are implementation details of effects that use particles, never separate entries or a user-facing "particle engine" toggle.
- Studio Show Modes retains one effects/sequence/preset editor. No competing Bluetooth FX editor, separate CORE-native effects selector, or WLED duplicate effect list. The **same** chosen effect ID and parameter model route through the selected controller transport.
- BLE is the primary runtime control: capability negotiation, supported FX IDs, zone assignments, speed/intensity/color/palette/custom properties, preset transfer, Stop and status. Phone does **not** send LED frames or calculate effects.
- Local presets and permitted autonomous trigger/auto-event playback continue when Bluetooth disconnects. A deliberate temporary/live BLE test may stop at its timeout or disconnect. Disconnection must never disable physical road-lighting inputs.
- Wi-Fi may remain optional for setup/development/legacy migration, but is **not required for ongoing effects**. Existing alpha Wi-Fi authorization and app transport switches are legacy; secure BLE onboarding is a separate production prerequisite.
- Only supported firmware modes are selectable. The upstream effects catalog is discovery metadata, **not** proof that 201 modes are implemented. WLED's 2D-only effects require separate dimensional support and should be hidden or marked unavailable for 1D strips.
- Priority: **hardware fallback and physical road-lighting triggers > startup/shutdown animations > decorative effects**. Rendering and BLE must never block input acquisition or override a required brake/turn/reverse response.

## One registry and one control path

```
Studio: Effects / Sequences / Presets (one catalog, one settings editor)
        ↓   small BLE commands + confirmed acknowledgements
CORE: secured BLE GATT → validated effect ID/settings → zone scheduler
                                            ↓
       internal effect renderer → optional particle physics → RGB framebuffers
                                            ↓
       trigger arbitration / hardware fail-safe → output driver → WS2815
```

The renderer's algorithm class (basic/particle/audio-reactive) is **internal** and shall not create a second UI section. Each effect has exactly one name/ID and its metadata controls the editor.

## Current code state and migration

- CORE's original five volatile native demo modes use a legacy BLE SHOW v1 command. Android currently injects a second independent five-mode BLE panel. **This is transitional, NOT the approved final interface.**
- WLED compatibility adapters currently cover a limited number of algorithms and were not connected to the physical LED rendering at initial import. The standalone host particle adapter in `main/core-firmware/effects/` is not yet an integrated native particle backend.
- Keep backward compatibility until one editor can send BLE native FX IDs. **Do not add these new FX IDs to another UI menu or duplicate effects in the app.**
- Implement BLE FX capability negotiation/versioning. Reject an unsupported ID/parameter rather than silently rendering a different effect.
- Verify OTA/boot/disconnect, nonvolatile presets, simultaneous zones, output NVS persistence, measured frame timing and independent fallback before production claims.

## Acceptance checks (bench, not road-certified)

1. BLE selects a supported ID, speed, intensity, zone, colors; **no Wi-Fi connected**; CORE renders actual LEDs.
2. The same effect ID is visible once in Studio; particle-based modes have no separate category or engine switch.
3. Disconnect BLE during a saved preset: CORE keeps rendering. Temporary live test can expire, but road-trigger response remains immediate.
4. Priority checks Brake + Hazards, Reverse, Parking and independent left/right; rendering never suppresses them.
5. Verify 12 V WS2815 BRG output on the user-configured zones; measure both buses' FPS and memory rather than assuming 60–120.
6. Ported WLED license EUPL-1.2-or-later attribution and source obligations reviewed before distribution.

Not yet validated by writing this architecture document.
