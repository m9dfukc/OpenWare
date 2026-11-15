# OpenWare – AC/DC fork (branch `acdc/thru-usb-feedback`)

This branch is an unofficial firmware for the **Rebel Technology / Befaco AC/DC** Eurorack USB audio interface. It is based on OpenWare and has two goals:

1. **Standalone use:** without a computer, the patch in slot 1 runs and its output reaches the jacks. A stored FX patch can be used without USB.
2. **USB without clicks:** with a computer the module works as a normal audio interface. The USB clock no longer drifts, so there are no lost blocks or clicks.

The USB routing is the stock one: the computer records the **patch output**. If you want the computer to record the raw inputs instead, see branch [`acdc/fx1d`](https://github.com/m9dfukc/OpenWare/tree/acdc/fx1d).

It has been built, flashed and tested on one AC/DC module with macOS (USB full speed, UAC 1.0, 4 in / 4 out, 48 kHz). It is not an official release. Use at your own risk and **back up your current firmware before flashing** (see below).

Discussion: [AC/DC as FX when not connected to computer (#2244)](https://community.rebeltech.org/t/ac-dc-as-fx-when-not-connected-to-computer/2244) · background on the noise: [Clock noise in AC/DC (#2072)](https://community.rebeltech.org/t/clock-noise-in-ac-dc/2072) · firmware update issues: [AC/DC firmware update (#2083)](https://community.rebeltech.org/t/ac-dc-firmware-update/2083)

## Branches

| Branch | Firmware version | Content |
|---|---|---|
| **`acdc/thru-usb-feedback`** (this one) | `v22.4.r-thru1d` | Standalone pass-through + USB fixes. The computer records the patch output (stock routing) |
| [`acdc/fx1d`](https://github.com/m9dfukc/OpenWare/tree/acdc/fx1d) | `v22.4.r-fx1d` | as this branch + the computer records the raw inputs, USB product string `OWL-ACDC-FX` |

## Base: why v22.4.r and not v22.5.0

The base is upstream commit `db9d3dd` ("v22.4.r: MCLK=512fs", 2022-07-13) on Martin Klang's unmerged branch `hack/acdc-output-levels`, with the `Libraries` submodule at `9e8f64e`.

- That branch contains the AC/DC-specific work that never made it into a release: a reworked USB audio path (SOF sync, explicit feedback, circular buffers), block size 8, input gain 88, exact 48 kHz (MCLK 512 fs), SAI output drive off, 4 TDM slots, level LEDs and the codec high-pass filter off.
- The newer tip of that branch (`7f3cc41`, v22.4.s, MCLK 256 fs) gave **no audio at all** on the tested module. v22.4.r works.
- The official v22.5.0 `ACDC.bin` from GitHub is reported to break USB on the AC/DC ([#2083](https://community.rebeltech.org/t/ac-dc-firmware-update/2083)).

## Changes on top of `db9d3dd`

| Commit | Change | Why |
|---|---|---|
| `85dd465c` | **GCC 12 build fix:** linker symbols declared as `uint8_t` (backport of upstream `5108fc56`, `fe6eff4f`) | Types only. Needed to build with Arm GNU Toolchain 12.2 |
| `21837c5d` | **Revert `237c63d`** ("hack to prevent ACDC audio thru") | `237c63d` writes zeros to the DAC whenever no USB playback stream is active. Without a computer, or with a computer that is not playing audio, the module was silent, even though the default patch "ACDC" is a pass-through VCA. With the revert the patch output reaches the jacks again |
| `3cfa66dd` | **USB: read and reset the RX sample counter atomically** | The USB interrupt read and cleared a counter that the SAI DMA interrupt increments, so a block could be lost from the feedback count. Closes the race, but on its own it did not fix the drift below |
| `174ef59a` | **Default output gain 125** (was 114) | With 114 the pass-through level was about halved (−5.5 dB measured). 125 is the value of the firmware previously installed on the module. Stored settings still take precedence |
| `6753794f` | **USB: closed-loop explicit feedback using the RX buffer level** | See next section |
| `06430d10` | Version `v22.4.r-thru1d` | |

### USB feedback fix (`6753794f`)

The explicit feedback endpoint reported only how many samples the codec consumed (open loop).

- **Problem:** On macOS the host then delivers on average about 1 LSB of the 10.14 feedback value too little (measured 1.15–1.7 ppm). The RX buffer holds only 3 packets (576 samples), so it ran empty every 2–3 minutes. One block of 8 frames was lost each time: an audible click, and the loop-back delay grew by 8 samples. The device reported this as `rx unf … 4.0000`.
- **Fix:**
  - The firmware now measures the RX fill level right after each OUT packet, averages it per feedback period (32 ms) and smooths it over about 8 periods.
  - It adds a proportional correction (2 LSB per frame of deviation, clamped to ±32 LSB ≈ ±40 ppm) to the feedback value.
  - The target level is half-way between underrun and overrun. This adds about 0.5 ms of latency.
- **Result** in a 4-channel loop-back test (sines out → in, recorded over USB, phase tracked per channel): before 25 sample slips in 30 min, after none in 30 min.

## Routing

| Situation | Outputs (jacks) | Recorded by the computer |
|---|---|---|
| No computer, or USB connected but no audio stream | **patch output** (default patch: pass-through, knob = gain) | – |
| Computer streams playback | **host playback** (overwrites the patch output) | – |
| Computer records | host playback, see note | **patch output**, including the digital gain of the default patch |

- **Note on recording:** macOS also starts the playback stream when an app only *records* from the AC/DC, for example Audacity. VCV Rack always opens the device in both directions. While recording, the outputs therefore carry the host playback (silence if nothing is played), not the patch.
- **Switching:** stop the host stream (deselect the device, quit the app) and the patch output returns.
- **Gain knob:** with the default patch "ACDC" each knob is a digital gain, (2 · knob)², about ×1 at 12 o'clock. It also scales the USB recording, including the noise. At 0 the recording is digitally silent.
- **Name:** the USB product string stays `OWL-ACDC`, so existing setups keep working.

## Build

Requirements: Arm GNU Toolchain **12.2.Rel1** (`arm-none-eabi-gcc 12.2.1 20221205`) and GNU Make.

```sh
git clone -b acdc/thru-usb-feedback https://github.com/m9dfukc/OpenWare.git
cd OpenWare
git submodule update --init      # Libraries at 9e8f64e
make -C ACDC CONFIG=Release TOOLROOT=/path/to/arm-gnu-toolchain/bin/ all
# result: ACDC/Build/ACDC.bin (≈ 106 KB, must stay below 128 KB internal flash)
```

The tested binary (`v22.4.r-thru1d`, 105 972 bytes, SHA-256 `f522560eb3d3c329c37753a1ebfdb326e45bae02192f81e866693b363f044c28`) was built from `db9d3dd` plus the same changes applied as patch files.

- The code is identical to this branch, only some comments differ.
- `GIT_REVISION` is embedded in the binary. A build from the branch therefore has a different SHA.

## Flash (ST ROM DFU, front USB-B jack)

1. **Back up first.** Put the module into DFU mode and read back the 128 KB internal flash. Keep this file: it is the only way back to the firmware your module shipped with.

   ```sh
   dfu-util -d 0483:df11 -a 0 -s 0x08000000:131072 -U acdc_backup.bin
   ```

2. **Enter DFU:** either "Reset to bootloader" on the openwarelab web page (it sends the OWL SysEx `SYSEX_BOOTLOADER_COMMAND`, `0x7E`), or the boot pads on the board.
3. **Write:**

   ```sh
   dfu-util -d 0483:df11 -a 0 -s 0x08000000 -D ACDC.bin
   ```

   Optionally read back again and compare the first bytes with `ACDC.bin`.
4. **Leave DFU with a full power cycle:** case power off **and** USB unplugged, then case on, then USB.

Patches stored in QSPI flash and the device settings are not touched by flashing the internal flash. On the tested module, slots and storage were identical before and after.

## Measurements (one module, macOS, Eurorack case)

Levels are in dBFS of the USB stream (16 bit), inputs open, gain knobs at 12 o'clock.

- **Standalone output is clean:** The 1 kHz comb (≈ k · 1000 Hz) disappears from the outputs as soon as USB is unplugged. It is already present when USB is connected with no stream, so the USB start-of-frame (SOF) activity alone is enough. It follows the host controller's SOF: its frequency changes with the USB port, 999.87 vs 999.99 Hz.
- **Inputs, recorded through the default patch:** On the tested module the knobs at 12 o'clock were 0.4–1.6 dB below unity, which lowers all values accordingly.

  | Measure | Level |
  |---|---|
  | RMS ≥ 20 Hz | −79.3 dBFS |
  | A-weighted | −80.5 dBFS |
  | Broadband floor without lines | −84.6 dBFS |
  | 1 kHz comb (sum) | −83.2 dBFS |
  | fs/8 line at 6000 Hz (block rate at block size 8) | −85.7 dBFS |
  | Mixing products n · f_SOF + m · 6000 Hz | −92.4 dBFS |

  The discrete lines make up about 70 % of the noise power. An ideal notch bank would gain ≈ 5.3 dB.
- **Repeatability:** Two recordings in a row, with a new USB stream in between, agree within 0.1 dB. Two sessions about 5 h apart agree within ±0.6 dB.
- **Compared with the base without the USB changes:** The noise is ≈ 1.1 dB lower in total, the comb ≈ 1.7 dB lower, fs/8 ≈ 1.5 dB lower.
- **Known side effect:** Since the feedback fix, the mixing products are ≈ 4.8 dB higher than before (reproducible, still about 4–7 % of the noise). The likely cause is the extra work in the USB interrupt.
- **Ground loops:** A loop appeared only in a measurement setup where the AC/DC was on a computer port and an audio interface fed from its outputs was on a hub. With both devices on the same hub it was gone. No USB isolator was needed.
- **Not addressed:**
  - the 1 kHz comb in recordings (it is in the ADC data while USB is active)
  - the LED PWM line at ≈ 2321 Hz (visible with signal)
  - the fs/8 line

---

# OpenWare
Firmware for OWL devices

This repository contains firmware source code for several Rebel Technology devices and products, including Wizard, Alchemist and Magus.

Source libraries and drivers are contained in a git submodule. On first git checkout, do:

    git submodule init

To get the latest library version, do:

    git submodule update

To compile a project firmware, set the `TOOLROOT` in `Hardware/common.mk` and build with Gnu Make, either using the top level makefile (try `make help`) or by entering a project subdirectory.

Compiles with arm-gcc-none-eabi cross compiler. Debugging can be done with openocd.

STM32CubeMX `.ioc` files are available in the project subdirectories.
