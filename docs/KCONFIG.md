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
| `ESP_RTL_SDR_ESP_R820T2_IF_MODE` | choice | `..._IF_TABLE` | R820T2 profiles only: IF filter and IF per sample rate. `..._IF_TABLE` (driver table) or `..._IF_LIBRTLSDR` (`r82xx_set_bandwidth()`). See [R820T2 IF mode](#r820t2-if-mode). |

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
(reg `0x0a` bit 4, reg `0x0b` under mask `0xef`) and the IF for the sample rate at start and on
every rate change, then points the RTL2832 demod IF (page 1 `0x19..0x1b`) and the PLL LO
(RF + IF) at that IF and retunes. Two mappings
(`private/rtl_profile.hpp`, `rtl_r820t2_if_for_rate`):

| Choice | Source |
|---|---|
| `ESP_RTL_SDR_ESP_R820T2_IF_TABLE` (default) | The driver's own table. Its 300-1500 kHz rows are the commented-out "settings from Oldenburger" table in the [librtlsdr/librtlsdr](https://github.com/librtlsdr/librtlsdr) fork's `tuner_r82xx.c`; the 1800-5000 kHz rows were edited by hand. Rows are picked by the first bandwidth at or above the rate. That fork writes reg `0x0a` with mask `0x0f`, this driver with `0x10`, so the table's `0x0a` values never set bit 4 below 5 MS/s. Not what any librtlsdr computes. |
| `ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR` | `r82xx_set_bandwidth()` with bandwidth = sample rate, as osmocom [rtl-sdr](https://github.com/osmocom/rtl-sdr) and [rtl-sdr-blog](https://github.com/rtlsdrblog/rtl-sdr-blog) (identical code) compute it, so what a Linux `rtl_433` on the same dongle uses. |

| Rate | Table: `0x0b` / IF / demod word | librtlsdr: `0x0a` bit 4, `0x0b` / IF / demod word |
|---|---|---|
| 250k | `e8` / 1.700 MHz / `3c38e4` | 0, `e6` / 2.125 MHz / `3b471d` |
| 1024k | `ec` / 1.400 MHz / `3ce38f` | 0, `eb` / 1.700 MHz / `3c38e4` |
| 2048k | `8f` / 1.750 MHz / `3c1c72` | 0, `af` / 1.625 MHz / `3c638f` |
| 2560k | `6f` / 2.000 MHz / `3b8e39` | **1**, `6b` / 3.570 MHz / `381112` |
| > 5 MS/s | 6/7/8 MHz settings, same as librtlsdr | `6b` / `2a` / `0b`, 3.57 or 4.57 MHz |

The table is the default because it is what has run on hardware: at 915 MHz / 2048k on a
Nooelec SMArt v5 it took utility-meter decodes from 0 to 5 in ten minutes (Linux `rtl_433`, same
dongle model: 10), with 433.92 MHz unchanged. librtlsdr's mapping has not been tried on the ESP
yet; the procedure below compares them.

The driver logs the choice at INFO on every rate change, e.g.
`r820t2 IF for 2048000 S/s: filter r0a=c5 r0b=af (was ...) if=1625000 Hz demod=3c638f`.

### Selecting it from ESPHome (esphome-rtl433)

The driver is an ESP-IDF component there, so its Kconfig takes `sdkconfig_options`:

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: y   # leave out for the table
```

A component can set it with `add_idf_sdkconfig_option("CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR", True)`.
Outside Kconfig, defining `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR` for the component's
sources does the same. Changing it changes `sdkconfig`; clean the build (`esphome clean`) if the log
line above does not change.

### A/B procedure (real hardware)

Goal: decide whether `..._IF_LIBRTLSDR` should replace the table, from decode counts and SNR
measured against a Linux `rtl_433` running on its own Nooelec at the same time (the baseline that
absorbs how busy each band happens to be).

1. **Build both firmwares from this one checkout**, so the mapping is the only difference: clone
   the branch into a directory named `esp_rtl_sdr`, set
   `ESP_RTL_SDR_LOCAL_PATH=/path/to/esp_rtl_sdr` when running `esphome`, and build once without and
   once with `CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: y`. Keep the gain on auto (no `gain:`),
   add `extra_args: ["-M", "level"]` so every event carries `rssi`/`snr`/`noise`, and use one band
   per run (a single `frequencies` entry, no hopping) so both setups see the same dwell:
   - 433.92 MHz, `sample_rate: 250k`, `decoders: [acurite_txr, acurite_606]`
   - 915 MHz, `sample_rate: 2048k`, `decoders: [scmplus]`
2. **Check the build** after each flash on a serial console with `CONFIG_LOG_DEFAULT_LEVEL_INFO`:
   the `r820t2 IF for ...` line must show the table's or librtlsdr's `r0b`/`if` from the table
   above (250k: `e8`/1700000 vs `e6`/2125000; 2048k: `8f`/1750000 vs `af`/1625000; `r0b` may show
   bit 4 (`0x10`) from the register's previous state).
3. **Baseline**: on the Linux box, for the same window and band,
   `rtl_433 -f 915M -s 2048k -R scmplus -M level -F json:linux-915-<mode>-<n>.json` (or
   `-f 433.92M -s 250k -R <acurite protocol numbers>`), auto gain. Antennas where they normally live.
4. **ESP capture**: `websocat ws://<device>:8433/ws > esp-915-<mode>-<n>.json` started at the
   same moment (drop anything timestamped before the window: `/ws` replays recent events on
   connect). `http://<device>:8433/cmd?cmd=get_stats` at the start and end gives the frame counts
   as a cross-check.
5. **Windows**: 915 MHz 30 min, 433.92 MHz 15 min, in the order table, librtlsdr, librtlsdr, table
   (ABBA) per band so a change in traffic over the session hits both modes equally.
6. **Score** each window:
   - decodes: `jq -c 'select(.model)' f.json | wc -l`, and distinct meters/sensors:
     `jq -r 'select(.model) | "\(.model) \(.id)"' f.json | sort -u | wc -l`;
   - ratio ESP / Linux for both;
   - SNR: median `.snr` over devices both receivers decoded,
     `jq -r 'select(.model) | "\(.model) \(.id) \(.snr)"'`, as ESP minus Linux.
7. **Decide**: switch the default (and esphome-rtl433's setting) to librtlsdr only if, averaged over
   its two windows, the 915 MHz ratio is at least the table's and 433.92 MHz loses no more than
   ~1 dB median SNR or any sensor. If librtlsdr wins at 915 but costs SNR at 433.92, the two
   rates could take different mappings; that would be a follow-up, not a Kconfig setting.

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
