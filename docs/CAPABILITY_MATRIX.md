# Capability matrix — esp_rtl_sdr vs desktop librtlsdr-class drivers

Desktop reference: **librtlsdr / rtl-sdr-blog**. Status matches `PROJECT_TRUTH.md`.

| # | Desktop capability | librtlsdr (typical) | esp_rtl_sdr 0.9.1 | Phase |
|---|---|---|---|---|
| 1 | Open / close | `rtlsdr_open` | `install` / `uninstall` | **Done** |
| 2 | Async IQ | `read_async` | `start` + `EVT_IQ_BLOCK` | **Done** |
| 3 | Sync IQ | `read_sync` | `read` | **Done** |
| 4–5 | Center freq | set/get | set/get + retune | **Done** |
| 6 | Sample rate | many rates | continuous in HW windows + quantize | **Done** (0.7) |
| 7 | Get sample rate | yes | exact programmed SPS | **Done** |
| 8–9 | Tuner gain | modes / steps | V4 28-step, V4L/V3c 29-step nominal manual ladders; board-specific Tuner AUTO and RTL AGC. V3c HF direct-Q bypasses tuner gain. | **Driver implemented; P4 acceptance open** |
| 11 | ppm | yes | software LO offset | **Done** |
| 12 | Bias-T | common | V4/V4L/user-tested V3c GPIO ON/OFF and no-load DC measured; OFF default/stop/reattach. BlogV3 identity is generic, so UI must warn before explicit enable. Loaded current unmeasured. | **Driver implemented; P4 acceptance open** |
| 12a | Tuner bandwidth | profile | At 2.4 MS/s, seven V4/V4L/V3c native-route and four V4/V4L HF-route width requests are mapped with paired PLL/demod IF. V3c boots on its proven 3.57 MHz IF and returns to that IF, with its boot filter registers, when AUTO is selected; every demod IF write is followed by a read. V3c direct-Q HF remains unsupported. Live EP0 failure rolls back or faults. Analog passbands unmeasured. | **Hardware-verified on the M5 Tab5 (V3c, V4L, V4, 96.1 MHz, 2026-09-28: see `docs/captures/v3c_live_bandwidth_2026-09-28.md`); other stations and long soak open** |
| 13 | Direct sampling / HF | forks | V4 and V4L use separate RF+28.8e6 upconverter routes by default; V3/V3c uses Q-branch direct sampling below 24 MHz. V4 and V4L can optionally tune the R828 directly below 28.8 MHz: `esp_rtl_sdr_set_hf_direct_min_hz()` / `esp_rtl_sdr_get_hf_direct_min_hz()` (default `0` keeps the upconverter; a nonzero threshold, at least 24 MHz, selects the direct input from that frequency up to 28.8 MHz). Nooelec remains fail-closed. | **Driver implemented; P4 RF acceptance open** |
| 15 | Multi-device | index/serial | yes | **Done** |
| 19 | Metrics | app-side | `get_metrics` | **Done (stronger)** |
| 20 | Capability bits | weak | `get_capabilities` | **Done (stronger)** |
| 21 | Fail-closed lifecycle | ad hoc | state machine | **Done (stronger)** |
| 25 | Intent / mission presets | no | `apply_need` | **Done (novel)** |
| 26 | On-host rate passport | no | `probe_rates` | **Done (novel)** |
| 27 | Health narrative | no | `get_health` | **Done (novel)** |
| 23 | Any RTL + any tuner | large table | profile-based; V4 only | 4 |
| 24 | Clean-room | N/A | **Yes** | Policy |

Novel rows (25–27) are **not** librtlsdr parity — they are deliberate ESP-native
advantages documented in `docs/VISION.md`.
