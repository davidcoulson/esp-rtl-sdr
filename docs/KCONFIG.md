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
| `ESP_RTL_SDR_ESP_R820T2_IF_MODE` | choice | `..._IF_TABLE` | R820T2 profiles only: IF filter and IF per sample rate. `..._IF_VENDOR_FIXED` (wide filter, 3.57 MHz, as the vendor DLL and upstream), `..._IF_TABLE` (driver table) or `..._IF_LIBRTLSDR` (`r82xx_set_bandwidth()`). See [R820T2 IF mode](#r820t2-if-mode). |

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

On the R820T2 profiles (Blog V3, Nooelec SMArt v5) the driver sets the tuner IF filter
(reg `0x0a` bit 4, reg `0x0b` under mask `0xef`) and the IF at start and on every rate change, then
points the RTL2832 demod IF (page 1 `0x19..0x1b`) and the PLL LO (RF + IF) at that IF and retunes.
Three mappings (`private/rtl_profile.hpp`, `rtl_r820t2_if_for_rate`), lettered as in the A/B below:

| | Choice | Source |
|---|---|---|
| A | `ESP_RTL_SDR_ESP_R820T2_IF_VENDOR_FIXED` | What the RTL-SDR Blog 1.4.0 DLL leaves on a Blog V3c (`d5/6b`) and a Nooelec v5 (`d3/6b`) after a cold open at **any** rate: wide filter, 3.57 MHz IF ([upstream captures](https://github.com/hardcoreerik/esp-rtl-sdr/blob/master/docs/captures/vendor_dll_rate_matrix_2026-10-01.md), two identical runs each). Upstream esp-rtl-sdr's cold start ends in the same state. Hot rate changes on these boards were not captured. |
| B | `ESP_RTL_SDR_ESP_R820T2_IF_TABLE` (default) | The driver's own table. Its 300-1500 kHz rows are the commented-out "settings from Oldenburger" table in the [librtlsdr/librtlsdr](https://github.com/librtlsdr/librtlsdr) fork's `tuner_r82xx.c`; the 1800-5000 kHz rows were edited by hand. Rows are picked by the first bandwidth at or above the rate. That fork writes reg `0x0a` with mask `0x0f`, this driver with `0x10`, so the table's `0x0a` values never set bit 4 below 5 MS/s. Not what any librtlsdr computes. |
| C | `ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR` | `r82xx_set_bandwidth()` with bandwidth = sample rate, as osmocom [rtl-sdr](https://github.com/osmocom/rtl-sdr) and [rtl-sdr-blog](https://github.com/rtlsdrblog/rtl-sdr-blog) (identical code) call it from `rtlsdr_set_sample_rate()`. The same values are in upstream's DLL captures of the Blog V4/V4L at every rate and of the Nooelec's explicit tuner-bandwidth choices. |

| Rate | A: `0x0b` / IF / demod word | B: `0x0b` / IF / demod word | C: `0x0b` / IF / demod word |
|---|---|---|---|
| 250k | `6b` / 3.570 MHz / `381112` | `e8` / 1.700 MHz / `3c38e4` | `e6` / 2.125 MHz / `3b471d` |
| 1024k | `6b` / 3.570 MHz / `381112` | `ec` / 1.400 MHz / `3ce38f` | `eb` / 1.700 MHz / `3c38e4` |
| 2048k | `6b` / 3.570 MHz / `381112` | `8f` / 1.750 MHz / `3c1c72` | `af` / 1.625 MHz / `3c638f` |
| 2560k | `6b` / 3.570 MHz / `381112` | `6f` / 2.000 MHz / `3b8e39` | `6b` / 3.570 MHz / `381112` |

Reg `0x0a` bit 4 is set in A at every rate, in C above 2.43 MS/s, and in B only above 5 MS/s.

B is the default because it is what has run on the owner's hardware: at 915 MHz / 2048k on a
Nooelec SMArt v5 it took utility-meter decodes from 0 to 5 in ten minutes (Linux `rtl_433`, same
dongle model: 10). That was measured against the replayed init's 2.2 MHz filter (`c5/8f`) under a
3.57 MHz IF (this fork's Nooelec skips the cold tuner reinit upstream runs), a mismatch that A, B and
C all replace; it says nothing about B against A or C. Neither A nor C has been tried on the ESP yet.

The driver logs the choice at INFO on every rate change, e.g.
`r820t2 IF for 2048000 S/s: filter r0a=c5 r0b=af (was ...) if=1625000 Hz demod=3c638f`.

### Selecting it from ESPHome (esphome-rtl433)

The driver is an ESP-IDF component there, so its Kconfig takes `sdkconfig_options`:

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_VENDOR_FIXED: y   # A; or ..._IF_LIBRTLSDR: y for C; neither for B
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
1. **Build three firmwares from this one checkout**, so the mapping is the only difference: clone
   the branch into a directory named `esp_rtl_sdr`, set `ESP_RTL_SDR_LOCAL_PATH=/path/to/esp_rtl_sdr`
   when running `esphome` (no downstream pin change needed), and build with
   `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_VENDOR_FIXED: y` (A), nothing (B), and
   `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: y` (C), `esphome clean` between them. Keep the
   gain on auto (no `gain:`), add `extra_args: ["-M", "level"]` so every event carries
   `rssi`/`snr`/`noise`, and use one band per run (a single `frequencies` entry, no hopping):
   - 433.92 MHz, `sample_rate: 250k`, `decoders: [acurite_txr, acurite_606]`
   - 915 MHz, `sample_rate: 2048k`, `decoders: [scmplus]`
2. **Check each flash** on a serial console with `CONFIG_LOG_DEFAULT_LEVEL_INFO`: the
   `r820t2 IF for ...` line must show the mode's `r0b`/`if` from the table above (250k: A `6b`/3570000,
   B `e8`/1700000, C `e6`/2125000; 2048k: A `6b`/3570000, B `8f`/1750000, C `af`/1625000). `r0b` may
   show bit 4 (`0x10`) from the register's previous state; `r0a` shows bit 4 set for A.
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
   or wins, it is what upstream already does (nothing to carry in the fork). If C wins, it can be
   proposed upstream from upstream's own captures (the V4 rate plan applied to the Nooelec,
   hardcoreerik/esp-rtl-sdr#53). B can only live in this fork. If one mode wins at 915 MHz and
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
