# Nooelec NESDR SMArt v5 profile: branch and eventual merge notes

Status: implementation branch `codex/nooelec-v5-profile`, based on master
`105caa5`, which is also the verified remote `v0.9.1` release commit
`105caa56b9b5ce395a7b4910c6f703c14b83b5d9`. This is a premerge review and acceptance record, not a claim that the change
is merged, released, or physically accepted. The release version/tag is not
selected. Consult the user before selecting or creating a release tag; merge
only with explicit approval, then verify the resulting commit on
`origin/master`.

The authorized scope is this driver repository only. No application changes,
integration work, or device writes were performed or are authorized by branch
readiness. The branch completes the internal Nooelec profile from first-party
September 30 PC control/IQ captures behind the existing public `esp_rtl_sdr`
API. Source correctness, capture transport, host regression tests, a targeted
ESP source compile, and physical RF behavior remain separate claims. Future
standalone hardware acceptance would be a separately scoped effort, not a
prerequisite silently added to this driver task.

[The capture note](captures/nooelec_v5_2026-09-30.md) gives the complete
campaign, selected frame anchors, tool and artifact hashes, rejected attempts,
and remaining evidence limits.

## Problem and concrete behavior changes

Master recognizes this dongle, but its provisional profile does not consistently
compose the independently measured Nooelec initialization, IF, route, and
control choices. In particular, master `105caa5` already uses a 3.57 MHz PLL
offset for Nooelec, while the replayed initialization ends with approximately
1.815 MHz demod IF and its Nooelec IF-restoration hook returns zero. That
mismatch shifts effective RF about **+1.755028 MHz above the dial**. A displayed
97.3 MHz can consequently correspond to about 99.055028 MHz.

The user reported hearing 99.1 MHz at a displayed 97.3 MHz and confirmed
OrcSDR used the latest driver tag, verified remotely as `v0.9.1`. The source
mismatch therefore affects the identified release; its matching sign and
approximate size make it a strong explanation to test. The exact OrcSDR image
hash/settings and actual device register trace remain unverified; no ESP
reproduction or hardware fix is claimed yet. The PC reference independently shows matched
cold 3.57 MHz and explicit AUTO 1.815 MHz paths. An older opposite PLL/demod
mismatch would shift in the opposite direction and is not the same explanation.

| Area | Master / provisional behavior | Branch behavior |
|---|---|---|
| Cold FM tune | PLL uses 3.57 MHz while replayed demod state can remain about 1.815 MHz | Restore matched 3.57 MHz demod IF and cold tuner state |
| R820-family initialization | Remapped table retains older model-specific `reg0a=d5` writes | Reuse the table and map the two Nooelec `d5` writes to captured `d3`; standby `36` remains intact |
| Native filter / IF after HF | No Nooelec-specific complete reinitialization policy | Restore captured `d3/6b` and standard IF when returning to the tuner |
| RF requests | Provisional Nooelec rejects requests below 24 MHz; shared upper bound is broader than its model rating | Model bounds 100 kHz through 1750 MHz; measured Q route below 24 MHz |
| Manual gain | Provisional profile does not advertise measured tuner gain | All 29 independently matched nominal native gain pairs, with route-aware availability |
| Tuner AUTO | No independently established Nooelec mode composition | Preserve cached gain nibbles while clearing `05` bit `10`, setting `07` bit `10`, and using `0c=6b` |
| Applied control state after reinitialization | Reinitialization can overwrite already applied gain/mode state | Preserve cold `0c=f0`, manual `0c=68`, or AUTO `0c=6b` as appropriate, and restore an applied manual gain/AUTO mode after native reinitialization |
| Final tune write | A common final `0c=68` write can undo Nooelec AUTO or alter its untouched cold state | Final tune uses `f0` cold / `68` applied manual / `6b` explicit AUTO, so hot retunes and bandwidth transactions preserve the selected mode |
| RTL AGC | Provisional profile does not advertise this control | Separate captured demod AGC off/on control |
| Explicit tuner bandwidth | No measured Nooelec plan exposed | Seven native choices with `reg0a=c3`, matched PLL/demod IF, and settle reads |
| Untouched default versus explicit AUTO | Cold boot and bandwidth-zero semantics can be conflated | Cold boot remains matched `d3/6b`, 3.57 MHz; an explicit AUTO request uses `c3/8f`, about 1.815 MHz |
| HF controls | No complete measured direct-Q profile | Independently matched Q sequence; bypassed-tuner gain/AUTO/bandwidth controls remain unavailable on that route |
| Board features | Shared VID/PID can invite unrelated board assumptions | No Nooelec bias tee and no V4 HF upconverter |

The [manufacturer's SMArt v5 datasheet](https://www.nooelec.com/datasheets/100701)
rates approximately 0.1–25 MHz direct sampling and 25–1750 MHz native tuner.
The observed PC route changes at **24 MHz**, not
25 or 28.8 MHz. The exact 24 MHz stimulus returns zero but prints
`[R82XX] PLL not locked!`. The software request range and routing rule are
documented policy, not proof of reception/lock at every edge. An optional
future driver hardware campaign should inspect the 24–25 MHz boundary and
report lock separately from the API return code.

## Older board behavior stays specific to those boards

The existing R820-family table, Q transition arrays, gain ladder, and bandwidth
transaction machinery are reused because Nooelec independently matches their
relevant wire behavior. This is not an assertion that the board is a V3c, V4,
or V4L. The vendor API reports the R820T family and tuner type 5; it does not
measure the die revision.

V3c keeps its older captured `d5/6b` cold filters and its intentional AUTO
restoration to 3.57 MHz. Nooelec uses `d3/6b` at cold start and `c3/8f` only
for explicit AUTO. V4L's older native plan uses `c4`; V4/V3 explicit native
plans use `c5`. V4/V4L HF upconverter and bias behavior remain tied to those
profiles. The board mapping must leave unrelated registers and the direct
standby byte `36` unchanged.

The existing V3/V4/V4L physical observations are older model-specific evidence,
not a Nooelec acceptance result. Likewise, Nooelec's PC captures do not replace
regression tests or physical checks of the older models after shared code
changes. The first-party clean-room boundary remains: public behavior and
our captures derive the implementation; foreign driver code is not imported.

## Exact tested scope and driver validation

The campaign already completed seven paired-control PC captures, 34,018
control submits, eleven cold FM sweep points, five identical settled cold
96.1 MHz PLL repeats, all 29 manual gain pairs, separate tuner/RTL AGC mode
requests, seven native bandwidth choices, live bandwidth and retune IQ,
eight cold direct-Q reads, long FM streaming holds, and powered-MLA AM IQ/audio.
All 204 failed control transfers were the four C8/C6 startup probes per open;
do not hide them behind zero API return codes or use them to claim features.

Those recorded PC results preceded this branch implementation. The capture
note also identifies a human-readable lab-report hash discrepancy: the current
report differs from its manifest entry. The branch owner freshly verified
all seven parent-PCAP hashes against the manifest; tool binaries and every
other artifact were not all freshly rehashed. Resolve the report provenance
item before publishing a new evidence manifest; do not say that the entire
evidence folder was freshly hash-verified.

The following narrow host validation ran with **GNU 13.3 under WSL**, from
the isolated driver checkout. The persistent build folder is `tests/host/build`:

```bash
cmake -S tests/host -B tests/host/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-Wall -Wextra -Werror'
cmake --build tests/host/build --target esp_rtl_sdr_profile_tests
ctest --test-dir tests/host/build -R '^profiles$' --output-on-failure
./tests/host/build/esp_rtl_sdr_profile_tests
```

The selected CTest run passed **1/1**, and the direct binary reported
**`RESULT profiles passed=1987 failed=0`**. This is the `profiles` target,
not a claim that the full suite ran. It covers the profile/control helpers and
retained older-profile expectations. The final runtime mode-restoration
changes described below require the separate runtime trace check; the earlier
host result must not substitute for that check. The post-fix source compile
has now passed as recorded below.

Truth hygiene also ran successfully. Because the repository script has CRLF
line endings, a normalized copy was retained outside the repository at
`C:\Tools\esp-rtl-sdr-lab\validation\nooelec-v5-20260930\check_truth_hygiene.sh`,
with `ROOT` set to this isolated worktree. The repository script was unchanged:

```powershell
wsl -e bash /mnt/c/Tools/esp-rtl-sdr-lab/validation/nooelec-v5-20260930/check_truth_hygiene.sh
```

Result: **`TRUTH_HYGIENE_OK ver=0.9.1`**. The reported version remains the
existing source version; this check does not select or create a release tag.

A targeted P4 source compile reused the cached **ESP-IDF 5.5.4** configuration
and substituted this worktree's include/source paths and a separate output:

```powershell
& C:\Tools\esp-rtl-sdr-lab\validation\nooelec-v5-20260930\compile-source-check.ps1
```

That check compiled **`src/esp_rtl_sdr.cpp` to one object successfully**, with existing
volatile increment/decrement warnings. It is not a full firmware link/build
or device result. The repeat against the final mode-restoration changes also
passed. Its retained log is
`C:\Tools\esp-rtl-sdr-lab\validation\nooelec-v5-20260930\compile-source-check-premerge.log`,
and the final object SHA-256 is
`6847E9C8A43DF765CB899808B43D9CCCEEB1F2C6F83952AD7689E22DE9C1098C`.
No application build or application integration is in scope.

Local review identified two concrete runtime state hazards. A final shared
tune write of `reg0c=68` could undo explicit Nooelec AUTO or change the
untouched cold state. Also, native tuner reinitialization could overwrite an
already applied gain/mode while getters still reported the cached selection.
The fixes compose the final register state as **f0 cold /68 manual /6b explicit
AUTO**, preserve that state through hot retunes and bandwidth changes, and
restore previously applied manual gain or AUTO after native reinitialization.
The final review also covered a queued AUTO-to-MANUAL request interrupted by
an HF retune. After successful manual restoration the driver clears the
Nooelec AUTO-applied flag, preventing the next tune from changing manual
`0c=68` back to `6b` while reporting MANUAL. Failed writes retain their error
return; the flag changes only after the manual writes succeed.
The added driver-runtime trace check compiles 19 actual driver functions with
successful mock USB transport. It does not substitute copied versions of the
tuning functions, and it does not claim to exercise the complete public API,
ESP task timing, failed USB transfers, or physical reception. Run from the
driver repository root:

```bash
python3 tests/scripts/test_nooelec_runtime_trace.py
```

The final Windows/WSL invocation was:

```powershell
wsl -e bash -lc 'cd /mnt/c/Users/hardc/.codex/worktrees/nooelec-v5-profile/esp_rtl_sdr && python3 tests/scripts/test_nooelec_runtime_trace.py'
```

Result: **`RESULT nooelec_runtime_trace passed=7664 failed=0`**. Checks cover
cold `83/75/f0` and `d3/6b`, matching settled 3.57 MHz IF, all 29 manual gain
pairs across native retunes and Q-to-native returns, Q enable/disable and NCO,
AUTO nibbles and `0c=6b` across retunes and all seven bandwidth choices,
explicit AUTO restoration after Q, the interrupted queued MANUAL case, and
RTL AGC write/settle pairs. The existing
Linux host-test CI step now runs this check; remote CI has not been run for
this local branch.

The following status table records the premerge validation state. The PR is
the durable record for subsequent CI and verified merge closure.

| Driver readiness item | Evidence / command | Result |
|---|---|---|
| Narrow diff and final source review | Focused local driver review, final diff and `git diff --check` | Gain/mode state findings and the queued-mode edge fixed and covered by runtime traces; whitespace check passed |
| Selected host profile regression | Commands above, GNU 13.3, `-Wall -Wextra -Werror` | CTest 1/1 passed; 1987 checks passed, 0 failed |
| Truth hygiene | External normalized script command above | `TRUTH_HYGIENE_OK ver=0.9.1` |
| Targeted P4 source compile | External `compile-source-check.ps1` above, cached IDF 5.5.4; final log/object hash above | Earlier and post-fix one-object compiles passed |
| Final runtime control composition | Exact Python/WSL commands above | 7664 checks passed, 0 failed |
| Full firmware link/build | Outside this driver's targeted validation | Not performed |
| Driver hardware / RF acceptance | Optional future separate effort below | Not performed; no on-device repair claim |
| Application integration / device writes | Outside the authorized driver-only scope | Not performed |
| Merge on master | Explicit approval, merge commit, refreshed origin verification | Not performed |
| Release | Consult user about version/tag before release actions | Not selected |

Keep tests narrow and results tied to the checked source state. Driver branch
readiness does not authorize integration or device writes. Host/compile success
does not establish physical reception or a repaired frequency display.

## Standalone driver API test cases

These cases describe the driver behavior to exercise through its public API
and a captured/mock transport. They need no application-specific code,
application build, serial-monitor command, or installation procedure. Final
runtime assertions belong in the driver trace test. Any live RF/hotplug test
would require a separate future hardware effort; no such test was executed
as part of this driver change.

For an optional physical campaign, record the driver revision, standalone
harness revision, dongle descriptors/serial, sample rate, requested RF,
applied mode/gain, RTL AGC, bandwidth request, PPM correction, antenna/power
arrangement, and cold/hot state. Retain ordered transfer traces and IQ.
Use the FM dipole for FM and the independently powered MLA-30+ with external
injector for HF. Antenna suitability and externally powered HF setup belong
with every RF result. No Nooelec bias-ON or V4 upconverter operation is valid.

| Case | API stimulus / initial state | Required driver-state result | Optional future RF observation |
|---|---|---|---|
| Identity | Enumerate exact Nooelec descriptors; contrast shared-VID/PID older boards | Select Nooelec only for its measured identity; retain older profile selection | Device starts and provides IQ under a recorded standalone setup |
| Cold native default | Fresh open at 96.1, 97.3, or 99.1 MHz, 2.4 MS/s, no explicit gain/bandwidth request | `d3/6b`, matched 3.57 MHz PLL/demod IF, cold `0c=f0`; no unintended mode/gain overlay | Reference signal is at the requested RF; investigate 99.1-versus-97.3 displacement separately from audio identity |
| Explicit bandwidth AUTO | Native route; apply AUTO after cold state or 200 kHz | `c3/8f`, IF `3b f7 78`, approximately 1.815 MHz; paired PLL/demod update and settle reads | Same RF placement before/after AUTO, without a roughly 1.755 MHz jump |
| Seven native widths | AUTO, 200, 300, 500, 1000, 1800, 2400 kHz | `reg0a=c3`; measured `reg0b` and IF map; 200/300 share a mapping | Stream continuity and frequency placement; analog passband calibration remains separate |
| Same-RF / step retune | 96.1 MHz baseline, 200 kHz, AUTO, explicit same-RF retune, +100 kHz, -100 kHz | Complete ordered bandwidth/tune state, not a partially updated IF; no retune required to repair a torn transaction | Match the original five-stage IQ comparison under the FM dipole |
| Manual gain | Native route; all 29 nominal values, including 0.0, 14.4, 22.9, 29.7, 49.6 dB | Captured 05/07 pairs and `0c=68`; retain applied manual state across native reinitialization | Stable IQ and no unexplained manual gain writes; no absolute dB calibration claim |
| Tuner AUTO | Toggle after high manual gain and after zero gain | Preserve cached low nibbles, clear 05 bit `10`, set 07 bit `10`, `0c=6b`; restore applied AUTO after native reinitialization | Stable reception under recorded settings; no inferred calibrated AUTO gain |
| RTL AGC independence | Toggle off/on/off separately from tuner AUTO | Page-0 register-19 `05/25/05` with settle reads; no implicit tuner-mode change | Record clipping/noise/reception separately from mode correctness |
| Cold Q | Fresh open at 1.28/1.6 MHz and the measured in-range cold-NCO points | Q input, captured NCO, tuner standby, no HF upconverter | Powered-MLA HF IQ; a PC-style 1.25/1.57 MHz center places AM1280/1600 around +30 kHz |
| Unsupported Q controls | Request tuner gain, tuner AUTO, or tuner bandwidth while direct Q bypasses the tuner | Fail/advertise unsupported as specified; do not change standby into a native tuner state | Do not claim controllable analog HF gain/bandwidth from an API return or tuner write |
| Hot route return | Native→1.28/1.6 MHz Q→native, with previously applied manual/AUTO/bandwidth state | Exact Q transitions; restore `d3/6b`, matched IF, applied mode/gain and applicable bandwidth on native return | Recorded FM/HF signal placement survives the transition under suitable antennas |
| Close / reopen / hotplug | Destroy/recreate standalone driver state, then mocked unplug/replug and re-enumeration | Distinguish fresh defaults from applied settings; clear stale device/register state; recover through measured profile initialization | Optional physical replug recovery, IQ continuity, and source-bounded observations |
| Bounds / cutoff | Below 100 kHz, above 1750 MHz, 23,999,999→24,000,000→25,000,000 Hz, and 28.8 MHz neighbors | Reject out-of-range requests; follow measured 24 MHz Q cutoff, not a V4 upconverter boundary | PLL lock and reception at 24–25 MHz remain distinct; PC logged a lock warning at 24 MHz |
| Older profiles | V3/V4/V4L identity, cold/hot routes, gain/mode/bandwidth helpers | Retain V3 `d5/6b` and 3.57 MHz AUTO, V4L `c4`, older V4/V4L routes/features; Nooelec restrictions stay local | Any new physical older-board regression remains a future separate observation |

The explanatory OrcSDR observation is retained as reported driver diagnosis
evidence for affected `v0.9.1`; it does not add application work to this scope.
Only actual standalone hardware evidence can close RF acceptance. Unperformed
physical tests remain limitations in support/release wording, not implied
authorization to undertake them while finishing the driver branch.

## Merge and release closure

The final runtime check, focused local driver review, and post-fix targeted
source compile have passed; exact results are recorded above. Before requesting
merge approval, verify the branch still contains only the reviewed changes.
Keep the targeted object compile distinct from
a full firmware build and the PC/host results distinct from physical RF.
The review lane should contain only this driver/profile work and its evidence.

A PR is not landed work. After explicit merge approval and the actual merge,
refresh origin and verify the merge commit on `origin/master` before calling
it landed. Update these notes with that commit and any subsequently supplied
driver evidence. The release version/tag remains unselected; consult the
user before choosing or creating a tag or taking a publication action.
