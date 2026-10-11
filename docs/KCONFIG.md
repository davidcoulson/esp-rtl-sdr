# Kconfig options — esp_rtl_sdr

**Menu path (menuconfig):** `Component config` → **ESP_RTL_SDR-ESP**  
**File:** [`Kconfig`](../Kconfig) at component root.

These options are read when the component is built under ESP-IDF. Defaults feed
`esp_rtl_sdr_config_default()` for transfer size/count when Kconfig is available.

---

## Options

| Symbol | Type | Default | Range / notes |
|---|---|---|---|
| `ESP_RTL_SDR_ESP_ENABLED` | bool | `y` | Master enable for the component. |
| `ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_BYTES` | int | `16384` | Bulk URB size in bytes. Must be a **multiple of 512** for HS bulk (validated at runtime). Range 512…262144. Gate-2 multi-URB default; some Tab5 paths used 32768 historically. |
| `ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_COUNT` | int | `6` | Number of driver-owned bulk buffers. Range 2…8. Higher = more USB pipeline depth, more RAM. |
| `ESP_RTL_SDR_ESP_LOG_VERBOSE` | bool | `n` | Extra driver logs (install path, URB noise). Keep off for production serial bandwidth. |
| `ESP_RTL_SDR_ESP_R820T2_IF_MODE` | choice | `..._IF_VENDOR_FIXED` | Blog V3 / Nooelec only: IF filter and IF per sample rate. `..._IF_VENDOR_FIXED` (A: wide filter, 3.57 MHz, the cold-start state), `..._IF_TABLE` (B: a downstream fork's table) or `..._IF_LIBRTLSDR` (C: `r82xx_set_bandwidth()`). See [R820T2 IF mode](#r820t2-if-mode). |

---

## How they interact with the C API

| Layer | Wins |
|---|---|
| Kconfig defaults | Fill `config_default()` when compiled under IDF with the symbols present |
| App `esp_rtl_sdr_config_t` | **Overrides** after `config_default()` — always preferred for product tuning |
| Runtime validate | Rejects illegal sizes (not multiple of 512, out of range) |

```c
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);   /* picks up Kconfig defaults under IDF */
cfg.transfer_bytes = 32768;         /* app override */
cfg.transfer_count = 4;
ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));
```

---

## Choosing transfer_bytes × transfer_count

| Goal | Suggestion |
|---|---|
| Continuous 960k FM-class (provenance path) | 6 × 16 KiB (defaults) |
| Higher SPS / fewer overruns | Try 6 × 32 KiB if PSRAM/internal allows |
| Tight RAM | 3–4 × 16 KiB; watch `get_metrics().overruns` |
| Passport soak | Prefer defaults for comparability |

Use `get_health()` / `get_metrics()` — not guesses — after changing URB geometry.

---

## R820T2 IF mode

How the Blog V3 and Nooelec SMArt v5 profiles set the tuner IF filter (reg `0x0a`/`0x0b`) and the
IF for the sample rate (`private/r820t2_if_mode.hpp`). The cold start always ends in A. For B and C,
start then applies the mode's plan the way an explicit tuner bandwidth is applied
(`run_bandwidth_program`: filter, PLL at RF + IF, demod IF with settle reads, band front end), and
hot retunes keep its IF (leaving direct sampling re-applies the whole plan). A rate change is a stop
and start here, so every rate gets its own plan. An explicit tuner bandwidth request (2.4 MS/s) still
wins, and the direct-sampling route below 24 MHz is untouched. Three mappings, lettered as in the
procedure below:

| | Choice | Source |
|---|---|---|
| A | `ESP_RTL_SDR_ESP_R820T2_IF_VENDOR_FIXED` (default) | What the RTL-SDR Blog 1.4.0 DLL leaves on a Blog V3c (`d5/6b`) and a Nooelec v5 (`d3/6b`) after a cold open at **any** rate: wide filter, 3.57 MHz IF ([vendor_dll_rate_matrix_2026-10-01.md](captures/vendor_dll_rate_matrix_2026-10-01.md), two identical runs each). This driver's cold start already ends there, so A adds no writes: it is the behaviour from before the option existed. Hot rate changes on these boards were not captured. |
| B | `ESP_RTL_SDR_ESP_R820T2_IF_TABLE` | The table davidcoulson/esp-rtl-sdr shipped in 59deff2. Its 300-1500 kHz rows are the commented-out "settings from Oldenburger" table in the [librtlsdr/librtlsdr](https://github.com/librtlsdr/librtlsdr) fork's `tuner_r82xx.c`; the 1800-5000 kHz rows were edited by hand. Rows are picked by the first bandwidth at or above the rate. Only bit 4 of its `0x0a` values is used, and it is clear below 5 MS/s. Not what any librtlsdr computes, and not clean-room evidence. |
| C | `ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR` | `r82xx_set_bandwidth()` with bandwidth = sample rate, as osmocom [rtl-sdr](https://github.com/osmocom/rtl-sdr) and [rtl-sdr-blog](https://github.com/rtlsdrblog/rtl-sdr-blog) (identical code) call it from `rtlsdr_set_sample_rate()`. The same values are in this repo's DLL captures of the Blog V4/V4L at every rate and of the Nooelec's explicit tuner-bandwidth choices (its 2.4 MS/s AUTO plan, `c3/8f`, 1.815 MHz, is C at 2.4 MS/s). |

| Rate | A: `0x0b` / IF / demod word | B: `0x0b` / IF / demod word | C: `0x0b` / IF / demod word |
|---|---|---|---|
| 250k | `6b` / 3.570 MHz / `381112` | `e8` / 1.700 MHz / `3c38e4` | `e6` / 2.125 MHz / `3b471d` |
| 1024k | `6b` / 3.570 MHz / `381112` | `ec` / 1.400 MHz / `3ce38f` | `eb` / 1.700 MHz / `3c38e4` |
| 2048k | `6b` / 3.570 MHz / `381112` | `8f` / 1.750 MHz / `3c1c72` | `af` / 1.625 MHz / `3c638f` |
| 2560k | `6b` / 3.570 MHz / `381112` | `6f` / 2.000 MHz / `3b8e39` | `6b` / 3.570 MHz / `381112` |

Reg `0x0a` is the board's captured base (Nooelec `c3`, V3 `c5`) with bit 4 set in A at every rate, in
C above 2.43 MS/s, and in B only above 5 MS/s.

B's only hardware result is downstream: at 915 MHz / 2048k on a Nooelec SMArt v5 it took utility-meter
decodes from 0 to 5 in ten minutes (Linux `rtl_433`, same dongle model: 10), but against the replayed
init's 2.2 MHz filter (`c5/8f`) under a 3.57 MHz IF, because that fork skipped the cold tuner reinit on
the Nooelec. It says nothing about B against A or C. Neither B nor C has been tried from this tree.

With B or C, start logs the plan at INFO, e.g.
`r820t2 IF mode=librtlsdr rate=2048000 r0a=c3 r0b=af if=1625000 Hz demod=3c638f`. With A there is no
such line; `demod IF restore ... demod_if_hz=3570000` is the last IF the start writes.

### Selecting it from ESPHome (esphome-rtl433)

The driver is an ESP-IDF component there, so its Kconfig takes `sdkconfig_options`:

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: y   # C; ..._IF_TABLE: y for B; neither for A
```

A component can set it with `add_idf_sdkconfig_option("CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR", True)`.
Outside Kconfig, defining the `CONFIG_` symbol for the component's sources does the same. Changing
it changes `sdkconfig`; clean the build (`esphome clean`) if the log line above does not change.

### A/B/C procedure (real hardware)

Goal: pick A, B or C for the Nooelec from decode counts and SNR measured against a Linux `rtl_433`
running on its own Nooelec at the same time (the baseline that absorbs how busy each band is).

0. **Find out what the Linux baseline programs.** By source, osmocom librtlsdr applies C on every
   rate change; the vendor DLL keeps the Nooelec at A on a cold open. On the Linux box:
   `sudo modprobe usbmon`, find the dongle's bus with `lsusb`, then
   `sudo tshark -i usbmon<bus> -w linux-2048k.pcapng` while starting
   `rtl_433 -f 915M -s 2048k` for a few seconds. Tuner writes are control OUTs with
   `wValue` `0x0034` and `wIndex` `0x0610` whose data starts with the register; demod IF writes have
   `wValue` `0x1920`/`0x1a20`/`0x1b20` and `wIndex` `0x0011`:
   `tshark -r linux-2048k.pcapng -Y 'usb.setup.wValue == 0x0034 && usb.setup.wIndex == 0x0610' -T fields -e usb.data_fragment | grep -E '^0[ab]'`
   (`usb.capdata` on older tshark). The last `0a`/`0b` written and the demod word tell which of
   A/B/C the baseline really is; record it with the results.
1. **Build three firmwares from one driver commit**, so the mapping is the only difference (pin the
   commit, or clone it into a directory named `esp_rtl_sdr` and set
   `ESP_RTL_SDR_LOCAL_PATH=/path/to/esp_rtl_sdr` when running `esphome`), with nothing (A),
   `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_TABLE: y` (B) and `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: y`
   (C), `esphome clean` between them. Keep the
   gain on auto (no `gain:`), add `extra_args: ["-M", "level"]` so every event carries
   `rssi`/`snr`/`noise`, and use one band per run (a single `frequencies` entry, no hopping):
   - 433.92 MHz, `sample_rate: 250k`, `decoders: [acurite_txr, acurite_606]`
   - 915 MHz, `sample_rate: 2048k`, `decoders: [scmplus]`
2. **Check each flash** on a serial console with `CONFIG_LOG_DEFAULT_LEVEL_INFO`: B and C log
   `r820t2 IF mode=table|librtlsdr rate=... r0a=c3 r0b=.. if=..` at every start (250k: B `e8`/1700000,
   C `e6`/2125000; 2048k: B `8f`/1750000, C `af`/1625000); A logs no such line, only
   `demod IF restore ... demod_if_hz=3570000`.
3. **Baseline**: on the Linux box, for the same window and band,
   `rtl_433 -f 915M -s 2048k -R scmplus -M level -F json:linux-915-<mode>-<n>.json` (or
   `-f 433.92M -s 250k` with the Acurite protocols), auto gain. Antennas where they normally live.
4. **ESP capture**: `websocat ws://<device>:8433/ws > esp-915-<mode>-<n>.json` started at the
   same moment (drop anything timestamped before the window: `/ws` replays recent events on
   connect). `http://<device>:8433/cmd?cmd=get_stats` at the start and end as a cross-check.
5. **Windows**: 915 MHz 30 min, 433.92 MHz 15 min, in the order A, B, C, C, B, A per band, so a
   drift in traffic over the session hits every mode equally (about 3 h at 915 MHz, 1.5 h at 433.92).
6. **Score** each window:
   - decodes: `jq -c 'select(.model)' f.json | wc -l`, and distinct meters/sensors:
     `jq -r 'select(.model) | "\(.model) \(.id)"' f.json | sort -u | wc -l`;
   - ratio ESP / Linux for both;
   - SNR: median `.snr` over devices both receivers decoded,
     `jq -r 'select(.model) | "\(.model) \(.id) \(.snr)"'`, as ESP minus Linux.
7. **Decide**, averaging each mode's two windows: the winner has the best 915 MHz ratio while losing
   no more than ~1 dB median SNR and no sensor at 433.92 MHz against the best mode there. If A ties
   or wins, keep the default. If C wins, set it downstream, and it can be proposed to
   hardcoreerik/esp-rtl-sdr from that repo's own captures (the V4 rate plan applied to the Nooelec,
   issue #53). B can only live downstream. If one mode wins at 915 MHz and
   another at 433.92 MHz, a per-rate split is a follow-up, not a Kconfig setting.

---

## What is *not* Kconfig (yet)

| Item | Where |
|---|---|
| USB vs delivery core pin | Compile-time constants — see [`RUNTIME_CONSTANTS.md`](RUNTIME_CONSTANTS.md) |
| Ring depth (pull IQ) | `kRingDepth` — same doc |
| Health emit period | `kHealthPeriodBlocks` — same doc |
| Rate windows / LO limits | Header macros + [`RATES.md`](RATES.md) |

Making those Kconfig is **Planned** only if product apps need it without rebuilding custom trees.

---

## menuconfig

```bash
idf.py menuconfig
# Component config → ESP_RTL_SDR-ESP
```

After changing defaults, clean rebuild if `sdkconfig` is already generated:

```bash
idf.py fullclean
idf.py build
```
