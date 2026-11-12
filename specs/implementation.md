# Test Framework: behavior and implementation

Describes the retained payload **0.1.0**. The behavior prose describes the retained payload; current candidate source and metadata are identified below. Gameplay qualification is a separate requirement.


<!-- source-only-sync -->
## Current source candidate

Source version **0.1.0** is saved from workshop revision `ca32bbf79ce0840d5166a232bdb3a9fdcd761254`. The retained installable payload remains **0.1.0** with its original bytes. This source update does not rebuild or requalify that payload. The current source and metadata define the candidate; gameplay, rendered behavior and co-op remain unqualified for rebuilt bytes.

The authored behavior and lifecycle descriptions below are retained from the published payload. They are not new qualification evidence for the source candidate. The integration table below follows the current candidate metadata.
<!-- /source-only-sync -->

## Behavior

A development-only bounded native scenario runner. Startup requires explicit test activation, a valid run ID, evidence location, expected isolated profile and selected cases. Registered cases execute through game-thread callbacks and report assertions/results with timeout and cleanup state. The separate isolation service checks actual profile/context admission before scenario mutations. It is not a normal player shortcut or automatic test-on-install feature.

## State and lifecycle

Case registry, run identity and execution state are temporary. Evidence records are run outputs; tests may intentionally mutate only an explicitly isolated disposable profile. Invalid activation/profile context refuses startup rather than choosing a personal save. World/thread/authority checks and cleanup gates remain authoritative throughout execution.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/test-framework/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Provides `test-framework.testing` | 1 | 24 bytes |
| Provides `test-framework.isolation` | 1 | 24 bytes |

## Implementation sources

- [mod.c](../source/mods/test-framework/mod.c)
- [core.h](../source/mods/test-framework/core.h)
- [stats_guard.h](../source/mods/test-framework/stats_guard.h)

## Verification boundary

Host/source checks, bootstrap cases and loaded-world scenarios are distinct. Missing prerequisites, timeouts or cleanup failures are failures/skips as recorded, never gameplay passes. No automatic game/profile preparation is included in this mod.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
