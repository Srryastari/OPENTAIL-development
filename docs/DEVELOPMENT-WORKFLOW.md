# OpenTail development workflow

OpenTail is an **internal development codename**, not a final product name.

## Required workflow
1. Import and verify historical Android/CORE/Studio source before changing those components.
2. Create a task branch from current `main` for each implementation.
3. Make small, reviewable changes and add tests.
4. Run applicable build, lint, and regression checks. Record skipped checks as **not run**, never as passing.
5. Update `CHANGELOG.md` with user-facing changes, and identify the exact release/version when known.
6. Open a pull request using `.github/pull_request_template.md`.
7. **Never merge to `main` automatically.** User approval is required for every PR. No unattended production firmware releases.
8. Changes involving road-light behavior, trigger priority, watchdog/failover, and vehicle power require explicit physical bench validation before claiming hardware readiness.

## Automation boundaries
- CI may run automatically on push/PR and publish build artifacts when configured.
- Scheduled Codex tasks may propose work and open PRs, but must not merge them.
- Scheduled reports must distinguish verified, inferred, blocked, and not tested.
- This document does not itself create Codex scheduled tasks or enable GitHub branch protection.

## Current blockers
- Historical releases are archived; active source extraction and version-specific build verification remain pending.
- Android and CORE buildable source trees are not yet confirmed in this repository.
- CI is preliminary; no claim of full firmware/Android test coverage.
