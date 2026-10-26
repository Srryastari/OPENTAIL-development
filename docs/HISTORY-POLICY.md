# Pre-GitHub development history policy

This repository began after significant development of the internally codenamed OpenTail project. We will preserve the full pre-repository development record, including historical source archives, Studio HTML releases, Android projects, CORE firmware, UI concepts, hardware designs, diagnostics, release notes, changelogs, and test evidence.

## Provenance and chronology
- **Original artifact**: byte-for-byte historical file from the user's ChatGPT Library or uploads, with original filename and SHA-256 digest.
- **Imported on**: date of the actual GitHub commit. This must never be confused with the original artifact creation or release date.
- **Historical release date**: use only an explicit, supported date from original file metadata or contents.
- **Reconstructed milestone**: an interpretation of the evidence, clearly labeled as reconstructed, not an authentic historical Git commit.
- **Verification**: distinguish host tests, user-reported physical bench tests, and untested claims.

## Import procedure
1. Inventory all Library items and additional user-supplied assets; don't assume a partial list is complete.
2. Materialize raw bytes, calculate SHA-256, and inspect archives for sensitive information and unsafe paths.
3. Commit original historical snapshots to versioned archive locations without overwriting.
4. Extract and compare latest verified projects before choosing active sources.
5. Construct a version chronology and changelog cross-referenced to archived artifacts and test evidence.
6. Use current-date Git commits for the import; never falsify historical author dates.
7. Report missing artifacts, unknown dates, duplicates, and validation gaps explicitly.

## Status
The inventory and some historical text documents are already committed. Full binary source and asset imports are pending. The initial latest Android, CORE, Studio, and hardware archives have been materialized for staging but are not yet committed. No claim of complete migration is made.
