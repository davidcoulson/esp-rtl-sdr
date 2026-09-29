# Versioning and release policy

`esp_rtl_sdr` uses [Semantic Versioning 2.0.0](https://semver.org). This document is the
rule set for choosing a version number, cutting a release, and how consumers such as
OrcSDR pin the driver. The header macros `ESP_RTL_SDR_VERSION_*` are the single source of
truth for the number; everything else is checked against them by
`tests/scripts/check_truth_hygiene.sh`.

## Format

    vMAJOR.MINOR.PATCH[-STAGE.N]        STAGE = alpha | beta | rc        N = 1, 2, 3, ...

Examples: `v0.9.1`, `v0.10.0-beta.1`, `v0.10.0-rc.2`, `v1.0.0`.

- `alpha < beta < rc < (no suffix)`. `N` is a separate dotted number, so `rc.10` sorts after
  `rc.9`. Do not write `rc3`; a glued number sorts as text and breaks at 10.
- The tag is the version prefixed with `v`. The header macros, `idf_component.yml`,
  `library.json` and the CHANGELOG use the version without the `v`.
- Tags before `v0.9.1` (`v0.7.x`, `v0.8.0-rc2`, `v0.8.0-rc3`) used a glued stage number. They
  stay as history and are never renamed or moved; the dotted form starts with `0.9.1`.
- `0.9.0` was never tagged or published. `0.9.1` is the first published 0.9 release and includes
  everything listed under 0.9.0 in the CHANGELOG.

## What changes the number

The rule is: pick the highest row that applies. While the major version is 0, an incompatible
public API change bumps MINOR (not MAJOR) and must be listed under **Changed (breaking)**.

| Change | Bump |
|---|---|
| Removed or renamed public function, changed signature or struct layout, renumbered enum/capability value, removed capability | **MAJOR** (pre-1.0: **MINOR**, marked breaking) |
| New public API function, new capability bit, new device profile, new supported sample-rate range, new tuner/gain/bias control, new Kconfig option | **MINOR** |
| Behaviour fix with no API change (wrong frequency, wrong register sequence, crash, leak, race, timing), performance work with no API change, promoting a provisional profile because of new code | **PATCH** |
| Documentation, tests, CI or examples only | none (goes out with the next release) |
| Adding hardware evidence only, no code change | none; update PROJECT_TRUTH.md in the next release |

When in doubt between two rows, take the higher one. A release that contains a MINOR change
resets PATCH to 0.

## Stages

| Stage | Meaning | Allowed changes |
|---|---|---|
| `alpha.N` | Incomplete; API and behaviour may change | anything |
| `beta.N` | Intended feature set is present; API or hardware behaviour may still change as validation grows | features, fixes |
| `rc.N` | Candidate for the exact stable `MAJOR.MINOR.PATCH`; scope and API frozen | release-blocking fixes, evidence, release docs only. Any code fix increments `N` |
| none | Stable | new work starts on the next version |

Each published build of the same core version increments `N`. A provisional device profile
does not turn a stable release back into a beta.

Private or experimental test builds do not get a stage. Identify them by the reported version,
full commit SHA and artifact SHA-256. Tags exist only for published releases.

## Where the version lives

Change all of these in the release commit (the hygiene script fails if they disagree):

1. `include/esp_rtl_sdr.h` — `ESP_RTL_SDR_VERSION_MAJOR/MINOR/PATCH`, `..._IS_PRERELEASE`, `..._PRERELEASE`
2. `idf_component.yml` and `library.json`
3. `README.md` (badge and status line), `PROJECT_TRUTH.md` (version line and release table),
   `docs/API.md`, `docs/API_REFERENCE.md`, `docs/CAPABILITY_MATRIX.md`
4. `examples/*/CMakeLists.txt` `PROJECT_VER` and the version check in `examples/p4_serial_smoke`
5. `CHANGELOG.md`

## CHANGELOG rules

- `## Unreleased` stays at the top. Every user-visible change is added there in the same PR that
  makes it, under **Added**, **Changed**, **Changed (breaking)**, **Fixed**, **Removed** or **Security**.
- At release, its entries move under `## X.Y.Z (date) — one-line summary`.
- State the evidence level with the claim. "Hardware-verified" needs the device, the setup and a
  link to the evidence; anything else says "implemented", "host-tested" or "not verified".
- Plan values from vendor captures are control choices, not measured analog passbands.

## Cutting a release

1. Branch `release/vX.Y.Z` from `master`. Bump the version everywhere above and move the CHANGELOG.
2. Open a PR to `master`. CI must be green: host tests on Ubuntu and Windows, truth/version
   hygiene, the ESP-IDF P4 compile. Run `coderabbitai review` if auto-review is off for the base.
3. Merge with a **merge commit**. Never squash or rebase a release PR or anything a consumer pins.
4. Create an **annotated** tag `vX.Y.Z` on the merge commit on `master`:
   `git tag -a vX.Y.Z -m "esp_rtl_sdr vX.Y.Z" <merge-commit>` and push it.
5. Create the GitHub release from the tag with the CHANGELOG section as the notes.
   `alpha`, `beta` and `rc` are always flagged **prerelease**; only a stable release is **Latest**.
6. Never move, delete or re-create a published tag. If a release is wrong, fix forward with the
   next PATCH and say so in its CHANGELOG entry.

## Hotfixes

Branch `hotfix/vX.Y.(Z+1)` from the tag being fixed (or from `master` if `master` has not moved),
open a PR to `master`, and follow the release steps with the next PATCH number.

## How consumers pin the driver

- **Development builds** (branches, PRs, `main` between releases) may pin a driver **branch and
  commit**: an immutable 40-character SHA, with the branch named in a comment.
- **Release builds** must pin a **published driver release**: the SHA of that release's tag, with
  the tag named in a comment (for example `# esp_rtl_sdr v0.9.1`). A SHA is used, not the tag
  name, because a tag can be moved and a SHA cannot.
- Never delete or force-push a branch or tag that a consumer pins. The driver keeps every pinned
  commit reachable.

The current OrcSDR-to-driver compatibility table is maintained in OrcSDR's `docs/VERSIONING.md`.

## 1.0.0

The criteria for `1.0.0` have not been defined. Until they are, `0.x` releases are stable in the
sense that a numbered release without a stage suffix has passed the release steps above; the
public API can still change in a MINOR release.