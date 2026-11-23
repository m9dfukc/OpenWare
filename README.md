# OpenWare – AC/DC fork (branch `acdc/fx1d`)

This branch is an unofficial firmware for the **Rebel Technology / Befaco AC/DC** Eurorack USB audio interface. It is based on OpenWare and has two goals:

1. **Standalone use:** without a computer, a stored patch runs and its output reaches the jacks. A stored FX patch can be used without USB. Which slot starts is chosen with the knobs in a boot menu and remembered, see "Boot menu" below.
2. **Normal USB interface with a computer:** the computer records the **raw inputs** (before the patch). Host playback goes to the outputs, and the USB clock no longer drifts. While recording, the input LEDs show the raw input level.
3. **Less noise:** the periodic code runs from ITCM. This lowers the fs/8 line by about 5 dB and removes the mixing products, see "ITCM hot path" below.

It has been built, flashed and tested on one AC/DC module with macOS (USB full speed, UAC 1.0, 4 in / 4 out, 48 kHz). It is not an official release. Use at your own risk and **back up your current firmware before flashing** (see below).

Discussion: [AC/DC as FX when not connected to computer (#2244)](https://community.rebeltech.org/t/ac-dc-as-fx-when-not-connected-to-computer/2244) · background on the noise: [Clock noise in AC/DC (#2072)](https://community.rebeltech.org/t/clock-noise-in-ac-dc/2072) · firmware update issues: [AC/DC firmware update (#2083)](https://community.rebeltech.org/t/ac-dc-firmware-update/2083)

## Branches

| Branch | Firmware version | Content |
|---|---|---|
| `acdc/thru-usb-feedback` | `v22.4.r-thru1d` | Standalone pass-through + USB fixes. The computer records the patch output (stock routing) |
| **`acdc/fx1d`** (this one) | `v22.4.r-fx8a` | as above + the computer records the raw inputs, USB product string `OWL-ACDC-FX`, input LEDs show the raw level while recording, hot path in ITCM, boot menu to choose the patch slot, MIDI send path fixed, LED mode (normal / clip only / off), LED PWM at 48 kHz, instruction cache on |

The branch name is historical: `v22.4.r-fx1d` is commit `11199347` on this branch (raw inputs to USB, LEDs unchanged), `v22.4.r-fx2d` is `abc3a72a` (raw input LEDs, code in flash), `v22.4.r-fx3l` is `5cb2c17c` (hot path in ITCM, slot 1 always starts), `v22.4.r-fx4b` is `2c963e3a` (boot menu), `v22.4.r-fx5a` is `c222fa49` (MIDI send path), `v22.4.r-fx6a` is `34782dea` (LED mode), `v22.4.r-fx7a` is `351b2071` (LED PWM at 48 kHz).

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
| `f7a255cc` | **`USE_USBD_AUDIO_TX_PRE_FX`**, product string `OWL-ACDC-FX` | The computer records the inputs before the patch. See "Routing" below |
| `11199347` | Version `v22.4.r-fx1d` | |
| `f206fdd4` | Input LEDs show the raw input level while the host records (first version, measured in `usbd_tx_convert`) | Superseded by `6d3b8bd5`, see "Memory layout matters" below |
| `6eee596c` | Version `v22.4.r-fx2b` | |
| `6d3b8bd5` | **Raw input LEDs without moving any other code or data** | See "Input LEDs" and "Memory layout matters" below |
| `cc2e08f8` | Version `v22.4.r-fx2d` | |
| `151bd8bc` | **Build with GCC 13+** (cherry-pick of upstream `d5b6409a`) | Compiler flags only. With GCC 12.2 the binary does not change |
| `2ae8b836` | **Start the audio block from the RX DMA interrupt** | Fixes stale input samples with a fast audio task, see "ITCM hot path" below |
| `0b7122de` | **Run the periodic hot path from ITCM** | See "ITCM hot path" below |
| `fb2c6ed9` | **USB audio: evaluate SOF only once per frame** | Fixes wrong feedback with the USB handler in ITCM, see "ITCM hot path" below |
| `5cb2c17c` | Version `v22.4.r-fx3l` | |
| `7ef6284a` | **Boot menu to choose the patch slot at power-on**, the slot is remembered | See "Boot menu" below |
| `2c963e3a` | Version `v22.4.r-fx4b` | |
| `bd04588e` | **MIDI: USB send path atomic, flow control for slot downloads** | See "MIDI send path" below |
| `c222fa49` | Version `v22.4.r-fx5a` | |
| `1cf56d3e` | **LED mode normal / clip only / off**, chosen with knob 2 in the boot menu and remembered | See "LEDs" below |
| `34782dea` | Version `v22.4.r-fx6a` | |
| `bd5ee463` | **LED PWM at 48 kHz, locked to the sample clock** (was 2320.54 Hz) | See "LEDs" below |
| `351b2071` | Version `v22.4.r-fx7a` | |
| `8a401e75` | **Instruction cache on** (was off in every Release build) | See "Instruction cache" below |
| `f9f9399a` | Version `v22.4.r-fx8a` | |

### USB feedback fix (`6753794f`)

The explicit feedback endpoint reported only how many samples the codec consumed (open loop).

- **Problem:** On macOS the host then delivers on average about 1 LSB of the 10.14 feedback value too little (measured 1.15–1.7 ppm). The RX buffer holds only 3 packets (576 samples), so it ran empty every 2–3 minutes. One block of 8 frames was lost each time: an audible click, and the loop-back delay grew by 8 samples. The device reported this as `rx unf … 4.0000`.
- **Fix:**
  - The firmware now measures the RX fill level right after each OUT packet, averages it per feedback period (32 ms) and smooths it over about 8 periods.
  - It adds a proportional correction (2 LSB per frame of deviation, clamped to ±32 LSB ≈ ±40 ppm) to the feedback value.
  - The target level is half-way between underrun and overrun. This adds about 0.5 ms of latency.
- **Result** in a 4-channel loop-back test (sines out → in, recorded over USB, phase tracked per channel):
  - before: 25 sample slips in 30 min
  - after: none in 30 min, and none in a further 10 min with this branch

## Routing

| Situation | Outputs (jacks) | Recorded by the computer |
|---|---|---|
| No computer, or USB connected but no audio stream | **patch output** (default patch: pass-through, knob = gain) | – |
| Computer streams playback | **host playback** (overwrites the patch output) | – |
| Computer records (`acdc/fx1d`) | host playback, see note | **raw inputs**, before the patch and its digital gain |
| Computer records (`acdc/thru-usb-feedback`, stock) | host playback | patch output |

- **Note on recording:** macOS also starts the playback stream when an app only *records* from the AC/DC. Some host software always opens the device in both directions. While recording, the outputs therefore carry the host playback (silence if nothing is played), not the patch.
- **Switching:** stop the host stream (deselect the device, quit the app) and the patch output returns.
- **Names:** with `acdc/fx1d` the audio device shows up as `OWL-ACDC-FX`. Saved setups in host software must select the device again. CoreMIDI may keep the old MIDI port name `OWL-ACDC` for a device it already knows. That is cosmetic.

## ITCM hot path (`v22.4.r-fx3l`)

Since the noise lines depend on where code sits in flash (see "Memory layout matters"), everything that runs periodically now runs from ITCM (zero wait state RAM at `0x3000`, part of CCMRAM), together with a copy of the vector table. This covers:
- the SAI DMA interrupts and `audioCallback`
- the FreeRTOS scheduler, idle task and SysTick
- the audio task (USB conversion, parameters, MIDI)
- the USB OTG interrupt, USB device core, audio class and feedback
- the ADC DMA, main loop and `onLoop` LED code

`Reset_Handler` copies vectors and code before anything else runs. Calls from ITCM back into flash go through linker veneers, and all of them are on error or rare event paths. The patch heap in CCMRAM shrinks from 52 KB to **39.3 KB**.

Moving code to ITCM exposed two timing bugs:

1. **Stale input sample:** the audio block was released by the TX DMA interrupt, but the RX DMA lags behind. With the faster audio task, the last input sample of every block (channel 4) was 16 frames old in 94–96 % of all blocks. Now the block starts from the RX DMA interrupt (`2ae8b836`).
   - A first fix that polled the RX DMA counter instead raised the fs/8 line by about 10 dB. A few µs of extra bus activity per block are enough.
2. **Double SOF:** with the USB handler in ITCM, the SOF callback ran twice in about 97 % of all frames, with the same frame number. The feedback period then covered only half the time. The host received wrong feedback (47.5–48.5 instead of ≈ 48.0), the RX buffer hit its limits and samples slipped. No ISO packet was lost.
   - Presumably the handler re-enters before the cleared `GINTSTS.SOF` flag takes effect in the USB clock domain.
   - Now each frame number is evaluated once (`fb2c6ed9`).

Results (one module, open inputs, gain at 12 o'clock, 2 × 60 s each, compared with `v22.4.r-fx2d` flashed back as a control; levels in dBFS of the USB stream):

| Measure | `v22.4.r-fx2d` | `v22.4.r-fx3l` | Difference |
|---|---|---|---|
| RMS ≥ 20 Hz | −77.6 | −78.7 | **−1.1 dB** |
| Broadband floor without lines | −83.2 | −84.5 | **−1.3 dB** |
| 1 kHz comb (sum) | −80.9 | −80.6 | +0.2 … +0.4 dB |
| fs/8 line at 6000 Hz | −84.3 | −89.4 | **−5.1 dB** |
| Mixing products n · f_SOF + m · 6000 Hz | −90.8 | ≈ −125 | **≈ −35 dB** (gone) |

- **Loop-back test** (sines out → in, 60 s): no sample slips, no stale samples, no feedback clamping.
- **Layout independent:** a build with 0x150 bytes of padding in flash (ITCM content byte identical) is within 0.25 dB in all measures.
- With the USB code in flash and everything else in ITCM, the comb was 3.5 dB higher than with `v22.4.r-fx2d`. The comb needs the USB code in ITCM.
- **In daily use since 2026-11-16 on the tested module.** Not yet checked: patches that need more than 39.3 KB of heap.

## Boot menu (`v22.4.r-fx4b`)

Stock OpenWare always starts slot 1 at power-on, and the AC/DC has no button to change the patch. So without a computer only one patch was usable. The boot menu uses the four knobs and the LEDs instead.

**Open the menu:** power on with knobs 1, 2 and 3 fully left and knob 4 fully right.

| Step | Action | LEDs |
|---|---|---|
| Menu open | (the knob pattern at power-on) | output LEDs lit, input LEDs show the remembered slot |
| Select | turn knob 1. Only occupied slots 1 to 15 can be selected, they share the knob travel evenly | input LEDs show the selected slot, output LEDs blink while confirming would change the slot |
| Confirm | turn knob 4 away from the right stop | the slot starts and is remembered |
| Cancel | turn knob 3 away from the left stop, or wait 30 s | the remembered slot starts, nothing is written |

- The input LEDs show the slot number in binary: LED 1 = 1, LED 2 = 2, LED 3 = 4, LED 4 = 8.
- Knob 1 only counts once it has been moved. Until then the remembered slot stays selected, so confirming by accident changes nothing. For the lowest slot, turn knob 1 away and back.
- Without the knob pattern the remembered slot starts, and its number is shown for one second after the start-up chaser.
- No patch runs while the menu is open, the outputs are silent.
- If the remembered slot is empty or nothing was remembered yet, slot 1 starts, or the first occupied slot.
- A Program Change from the computer still switches the running patch, but it is not remembered.

**Where the slot is stored:** in a 4 byte system resource `ACDC.slot` in the patch flash (QSPI). The application settings are not touched. Each change appends 256 bytes and marks the old record as erased, so the storage lasts for tens of thousands of changes. With less than 64 KB of free storage the firmware does not write any more: the slot starts without being remembered and the input LEDs flash red for a second. This keeps the storage from running into a defrag, which erases the whole patch flash.

**Implementation:** `acdcBootSelect()` in `ACDC/Core/Src/ACDC.cpp`, called from `bootstrap()` in `Source/ProgramManager.cpp`. It runs in the manager task before the first patch starts. The scheduler is cooperative, so the main loop (MIDI, watchdog) keeps running while the menu sleeps, and the module stays reachable over USB MIDI. The only code added to the periodic path is one test at the start of the `onLoop` LED code. The menu itself is ordinary flash code, the ITCM block grows by 40 bytes.

**Tested** on one module: the knob stops read 2 and 127 of 127 (7 bit), the thresholds are 5 % and 95 %. Selecting, confirming, cancelling with knob 3, the timeout and starting from the remembered slot work. USB loop-back (4 sines, 60 s): no slips and no stale samples, also with a patch that uses 53 % CPU. Noise against `v22.4.r-fx3l` in the same session (fx4b, fx3l, fx4b, 2 × 60 s each, open inputs): all four measures within ±0.4 dB, on average within ±0.2 dB.

## MIDI send path (`v22.4.r-fx5a`)

**The problem:** while a patch was running, downloading a stored slot over MIDI (SysEx `0x18`, as backup tools do) delivered damaged data in about one of four cases, and sometimes the module then stopped sending MIDI until a power cycle. Audio kept running. On the tested module with `v22.4.r-fx4b`: 19 of 79 downloads damaged, then the hang. The code is the same in upstream `master`.

**The cause:**
- The MIDI send buffer is written from three contexts: the audio task, the main loop and the USB interrupt (replies to SysEx requests are generated there). Nothing protected it.
- `transmit()` was called from the audio task in every block and from `write()` at every wrap of the buffer. `usbd_midi_tx()` silently sent nothing while a transfer was running, but the read head advanced all the same, so data was lost.
- Two contexts could start a transfer on the same endpoint twice. This is the likely cause of the hang, it has not been proven.
- The read head moved when a transfer started, although USB copies the data out of the ring buffer afterwards.
- `write()` never checked for a full buffer, and the slot download wrote 324 bytes every 2 ms without waiting for room.

**The fix (`bd04588e`):**
- Every access to the buffer state runs with interrupts masked, for about a microsecond. Check, start and bookkeeping of a transfer are one piece.
- The read head only moves once the transfer has completed.
- A full buffer drops the new message instead of overwriting unsent data, and reports `midi tx ovf` once as the program message.
- `transmit()` leaves without masking while there is nothing to start, so the audio task pays nothing in the idle case. The function stays in ITCM and grows by 48 bytes.
- The slot download only queues a message that fits as a whole and gives up after 5 s without room.

**Not covered:** a reply generated in the interrupt can still fall into the middle of a SysEx sent from the main loop. This only happens if the computer sends a request during a download. `Source/usbd_midi.c` (other devices, bootloader) gets the same contract, but only the AC/DC was built and tested.

**Tested** on one module, against `v22.4.r-fx4b` in the same session: 240 of 240 downloads intact and no hang, with patches using 32 % and 51 % CPU and with an audio stream running. USB loop-back (4 sines, 60 s): no slips and no stale samples, also while 40 downloads were running. Noise with open inputs (fx4b, fx5a, fx4b, fx5a, 2 × 60 s each): RMS, residual, comb and fs/8 within ±0.5 dB in every block. The mixing products were +0.7 dB on average, while the two fx4b controls differed by 2.3 dB, so this session cannot decide them.

## LEDs: mode and PWM rate (`v22.4.r-fx6a`, `v22.4.r-fx7a`)

**The problem:** every lit input LED shifts the zero point of **all** inputs by about −2.9e-4 full scale (≈ −2.9 mV, −71 dBFS), measured in the raw ADC data by a patch on the module itself. All inputs move alike, so the LED current probably shifts a common reference of the converter (not verified). A steadily lit LED is only an offset. Every change is a step, about 30 dB above the comb lines of the raw inputs. A dimmed LED repeats the step at the PWM rate: that is the line at 2320.54 Hz with harmonics, at −115 dB in the raw inputs with open inputs and up to −40 dBFS at an output when a patch dims an LED and passes its input on. The level LEDs follow the signal, so this is in every recording.

**LED mode (`1cf56d3e`, `v22.4.r-fx6a`):** in the boot menu (see above) knob 2 now selects an LED mode: left third **normal** (as before), middle third **clip only** (all dark, an input LED turns red only while the computer records and a raw input reaches −0.1 dBFS, held 300 ms), right third **off**. Like knob 1 it only counts once it has been moved, otherwise the remembered mode stays. The output LEDs show the mode in the menu (four, two or none lit). The mode is remembered in its own 4 byte resource `ACDC.led`, separate from `ACDC.slot`, with the same 64 KB reserve. In the dark modes the main loop switches the LEDs off once and then writes no pin or timer register while nothing changes; the new code sits in the padding of `.text.onLoop`, every other function in ITCM keeps its address.
- Measured: in mode off the LED shift of the inputs drops to 1.3–1.7 % (about −37 dB), also without the menu after a power cycle. In mode clip only the inputs jump by about −0.2 mV exactly when the red LED switches on, 220 ms before the converter clips: the red LED is the step, not the overload.

**LED PWM at 48 kHz (`bd5ee463`, `v22.4.r-fx7a`):** the LED timers TIM2/3/4/8 now run with prescaler 4 and period 999: 240 MHz / 5000 = 48 000 Hz instead of 240 MHz / (101 × 1024) = 2320.54 Hz. The timers run from PLL1, SAI1 from PLL3, both from the same 8 MHz crystal, so the PWM sits 0.1 ppm above the sample rate. `setLed()` (in ITCM) is unchanged: `pwm = 1024 − v` with an auto-reload of 999 keeps off off and full on at 999/1000, values below 2.4 % stay dark. Only the timer init changes, the ITCM content is byte identical with `v22.4.r-fx6a`.
- Measured: with a patch dimming three LEDs to 60 %, the line at an output fell from −39.8 dBFS to the noise floor (−85 dBFS, so at least 45 dB). In the raw inputs the 2320.54 Hz line found with `v22.4.r-fx6a` (−115 dB) is gone. No new line appeared, also not below 20 Hz.
- **Not changed:** the slow shift with the LED brightness stays. Against that only the modes clip only and off help.

**Tested** on one module: noise of `v22.4.r-fx6a` (mode normal) against `v22.4.r-fx5a`, and of `v22.4.r-fx7a` against `v22.4.r-fx6a`, each with controls before and after: no difference beyond the session drift (`v22.4.r-fx7a` within 0.15 dB of `v22.4.r-fx6a` in RMS, residual, comb and fs/8). USB loop-back (4 sines, 60 s): no slips and no stale samples with both. 240 of 240 slot downloads intact with `v22.4.r-fx6a`.

**Known issue (older than these builds, also in `v22.4.r-fx5a`):** a Program Change while the USB audio stream is open breaks playback until the next stream start (about 10 % silence on all outputs, the device reports `fb clamp`). Change the patch before opening the stream.

## Instruction cache (`v22.4.r-fx8a`)

**The caches were off.** `ACDC/Core/Inc/hardware.h` defines `USE_ICACHE` and `USE_DCACHE` only `#ifdef NDEBUG`, and the Release build (`Hardware/owl3.mk`) never defines `NDEBUG`. This is the same in v22.5.0 and upstream `master`. On the module, SCB->CCR reads 0 under `v22.4.r-fx7a`: neither cache is on. Patch code was fetched over the bus for every instruction. A plate reverb patch linked to AXI SRAM needed 5000 cycles per sample for about 1000 instructions, a plain read loop took about 24 cycles per iteration whether the data was in ITCM, DTCM or AXI SRAM.

**Change (`8a401e75`):** `USE_ICACHE` is always defined. `USE_DCACHE` stays behind `NDEBUG`, so the data cache stays off: it needs an MPU region that keeps the DMA buffers uncached. That region in `Source/mpu.c` only concerns data and is therefore tied to `USE_DCACHE` only (it was `USE_ICACHE || USE_DCACHE`).
- Correction: the commit message of `8a401e75` and a comment in `hardware.h` say that `_DMA_DATA` is not defined in the ACDC linker script. That is wrong. `Hardware/xibeca.ld`, which the ACDC uses, defines it with `PROVIDE` at the start of `.dmadata` (0x30000000, 64 KB); it was only missing from the ELF because nothing referenced it. The DMA region in `mpu.c` also uses TEX=1, C=0, B=1, a reserved encoding in ARMv7-M (Normal non-cacheable is B=0). Both matter only for the data cache.
- The firmware already invalidates the I-cache after copying a patch and before starting it (`device_cache_invalidate()` in `PatchDefinition.hpp`) and before jumping to the bootloader. With `USE_ICACHE` these calls now do something.
- ITCM and DTCM bypass the cache. The hot path in ITCM is unchanged (byte identical with `v22.4.r-fx7a`). Patches linked to DTCM (`patchram`, the default of the patch library) fetch their instructions as before and are not faster; patches linked to AXI SRAM (`plusram`) and firmware code in flash are.

**Measured** on one module:
- The plate reverb patch: 3540 instead of 5050 cycles per sample, 35 % instead of 51 % CPU. The read loop now shows the data paths themselves: ITCM/DTCM 5–6, AXI SRAM 15 (no data cache), SDRAM 46 cycles per access.
- Patches in DTCM: unchanged (an allpass chain 70 % with `v22.4.r-fx7a`, 71 % with `v22.4.r-fx8a`, same knob positions).
- Program change through all twelve slots and RAM uploads in between: every patch runs and reports.
- Noise against `v22.4.r-fx7a` (`v22.4.r-fx8a` before and after, `v22.4.r-fx7a` in the middle): the fs/8 line at 6 kHz **+0.6 dB** in all four runs, mixing products +1 … +3 dB, RMS, residual and comb unchanged, no new line. USB loop-back (4 sines, 60 s): no slips, no stale samples. 240 of 240 slot downloads intact.

## Input LEDs (`6d3b8bd5`)

Each input has a green level LED and a red clip LED. The four green output LEDs are unchanged (the firmware measures the DAC data).

| Situation | Green input LED | Red input LED |
|---|---|---|
| No host recording (standalone, USB idle, or playback only) | value set by the patch. Default patch: mean square level **after** the knob gain | default patch: RMS above −1 dB after the knob gain |
| Host records (USB TX stream active) | **raw input level**, mean square of the ADC data, smoothed with τ = 40 ms. Same scale as the default patch, so at 12 o'clock (gain × 1) it looks the same | **ADC clipping:** on for 300 ms after a sample reached −0.1 dBFS |

- **Why:** while the host records, the knobs change neither the recording (raw inputs) nor the outputs (host playback). With the stock LEDs, turning a knob still changed the input LEDs, which was misleading.
- **Where:** `onLoop()` (every 2 ms) reads the codec DMA input buffer (both halves, 16 frames, about 1/6 of the signal), only while TX is active. Nothing in the audio path changes. The level is right on average; a single clip shorter than ≈ 2 ms can be missed, sustained clipping is caught.
- **Memory layout:** the new code is in `.late_text` (after everything else in flash), its state in `.late_bss` (top 64 B of RAM_D2, see `Hardware/xibeca.ld`). `onLoop()` is a jump padded to its original size. The binary is byte identical to `v22.4.r-fx1d` except for `onLoop`, one character of the version string and the value of `_HEAP_D2_SIZE` (64 B smaller); the new code is appended. Why: see "Memory layout matters" below.
- **Scale:** mean square is an audio-style display. A DC control voltage looks dim: 1 V ≈ 1 %, 5 V ≈ 28 % brightness (input full scale ≈ ±9.5 V measured on the loop-back, spec ±10 V).
- **Clipping:** the ADC reaches full scale at about ±9.5–10 V. Signals up to the ±12 V rails are safe, but recordings above ≈ 10 V are clipped, and that is what the red LED shows while recording.
- **Note:** SAI1 delivers the 24-bit samples right-aligned **without sign extension**. They are sign-extended before measuring. A first build without this (`v22.4.r-fx2`, not committed) showed the red LEDs permanently on while recording.

## Build

Requirements: Arm GNU Toolchain **12.2.Rel1** (`arm-none-eabi-gcc 12.2.1 20221205`) and GNU Make.

```sh
git clone -b acdc/fx1d https://github.com/m9dfukc/OpenWare.git
cd OpenWare
git submodule update --init      # Libraries at 9e8f64e
make -C ACDC CONFIG=Release TOOLROOT=/path/to/arm-gnu-toolchain/bin/ all
# result: ACDC/Build/ACDC.bin (≈ 107 KB, must stay below 128 KB internal flash)
```

The tested binary is `v22.4.r-fx8a`: 110 208 bytes, SHA-256 `e1b1097b21008486acc4cda4b446a0e72fcd2e974ef0f5b97c6a707e6086f39a`.
- A build of this branch with Arm GNU Toolchain 12.2.Rel1 and `CONFIG=Release` is byte identical to it.
- Earlier tested binaries: `v22.4.r-fx7a` (state at `351b2071`, 110 088 bytes, SHA-256 `cac6fee0f86426aed3d9b5bfaa2ff758b69de59ed7e11971f2a2ba0d0233ff20`), `v22.4.r-fx6a` (state at `34782dea`, 110 088 bytes, SHA-256 `6a3fc427b30ae1043b239f01e5ecf9894db92d9a6ca3dc51b47cc9a6f80260eb`), `v22.4.r-fx5a` (state at `c222fa49`, 109 524 bytes, SHA-256 `b69c25b3f469fc6fb793b16569c53a63a7333c32d83c96de4d97b6fb71bdb8ef`), `v22.4.r-fx4b` (state at `2c963e3a`, 108 496 bytes, SHA-256 `a21f35c990cc33737620597d883c4bd205fc4495bf2291a1b6013af6f2b06307`), `v22.4.r-fx3l` (state at `5cb2c17c`, 107 340 bytes, SHA-256 `b55b94d8734baf1cf763dfefb380a561029bd00d4a645cdcef3b75d99e6f010e`), `v22.4.r-fx2d` (state at `abc3a72a`, 106 788 bytes, SHA-256 `d212cf8def6f239db997cd45a73304db9bcaf981375e55fdfc2ca2862718832a`) and `v22.4.r-fx1d` (state at `11199347`, 105 968 bytes, SHA-256 `7f9f9a34b5b19a0245f0ab15426d891adc4f00505c86236cadec167e0402925b`).
- These two were built from `db9d3dd` plus the same changes as patch files. Their code matches the branch, only some comments differ.

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
4. **Leave DFU:** either read back once more with `:leave`, which makes the ROM DFU start the firmware (`dfu-util -d 0483:df11 -a 0 -s 0x08000000:131072:leave -U readback.bin`; dfu-util then reports an error because the device is gone, that is expected), or do a full power cycle: case power off **and** USB unplugged (USB alone powers the board and keeps it in DFU), then case on, then USB. On the tested module both gave the same noise.

Patches stored in QSPI flash and the device settings are not touched by flashing the internal flash. On the tested module, slots and storage were identical before and after.

## Test status

`v22.4.r-fx8a`: see "Instruction cache" above. Flashed and read back, version, slots and storage unchanged after flashing.

`v22.4.r-fx7a` and `v22.4.r-fx6a`: see "LEDs" above. Flashed and read back, version, slots and storage unchanged after flashing.

`v22.4.r-fx5a`: see "MIDI send path" above. Flashed and read back, version, slots and storage unchanged after flashing.

`v22.4.r-fx4b`: see "Boot menu" above. Flashed and read back, slots and storage unchanged after flashing.

`v22.4.r-fx3l`: see "ITCM hot path" above. Flashed and read back, version, slots and storage unchanged after flashing.

`v22.4.r-fx2d`:
- Flashed and read back, version, slots and storage unchanged after flashing.
- Input LEDs: without a host stream they follow the knobs as before; while recording they follow the raw input and the knobs have no effect on them.
- Red clip LED verified in the loop-back test (sine and DC up to full scale): on from ≈ −0.1 dBFS.
- **Noise** (open inputs, 2 × 60 s each, compared with `v22.4.r-fx1d` flashed back as a control in the same session):

  | Measure | `v22.4.r-fx2d` − `v22.4.r-fx1d` |
  |---|---|
  | fs/8 line at 6000 Hz | +0.05 dB |
  | Mixing products n · f_SOF + m · 6000 Hz | +0.03 dB |
  | Broadband floor without lines | −0.2 dB |
  | 1 kHz comb (sum) | +0.4 … +0.55 dB (rose by 0.26 dB between the two runs, session drift not excluded) |
  | RMS ≥ 20 Hz | +0.2 dB |

## Memory layout matters

The first two attempts at the raw input LEDs (`v22.4.r-fx2b`: measurement in `usbd_tx_convert`, and an unreleased variant measuring in `onLoop`) both raised the fs/8 line by 1.4–2.0 dB and the comb by 1.5 dB, and lowered the mixing products by 4.0–4.4 dB. A test build that is functionally identical to `v22.4.r-fx1d` but has 0x150 bytes of unused padding before the audio functions (and 0x28 bytes in RAM) reproduced this within 0.12 dB.

So on this module the **position of code and data in memory** changes the block-synchronous noise lines, without any functional change. The likely mechanism is a different CPU timing pattern within each audio block (flash wait states, prefetch, cache lines), coupling into the ADC through supply and ground. This is not verified.

Consequences:
- Any firmware change can move these lines by a few dB, even if it only touches code outside the audio path. Comparisons between builds measure the code **and** its layout.
- `v22.4.r-fx2d` therefore keeps every existing address and only appends code.
- Up to `v22.4.r-fx7a` both caches were off (see "Instruction cache"), so flash wait states hit every instruction fetch outside ITCM.
- `v22.4.r-fx3l` removes the dependence: with the periodic code in ITCM, 0x150 bytes of padding in flash change no measure by more than 0.25 dB (see "ITCM hot path").

## Measurements (one module, macOS, Eurorack case)

Measured with `v22.4.r-fx1d`. `v22.4.r-fx2d` matches it within the table in "Test status" above. For `v22.4.r-fx3l` see "ITCM hot path". Absolute levels vary by about 1 dB between sessions (host load, USB port), so compare builds only within one session.

Levels are in dBFS of the USB stream (16 bit), inputs open, gain knob at 12 o'clock.

- **Standalone output is clean:** The 1 kHz comb (≈ k · 1000 Hz) disappears from the outputs as soon as USB is unplugged. It is already present when USB is connected with no stream, so the USB start-of-frame (SOF) activity alone is enough. It follows the host controller's SOF: its frequency changes with the USB port, 999.87 vs 999.99 Hz.
- **Inputs with `acdc/fx1d`** (raw ADC data, independent of the knob):

  | Measure | Level |
  |---|---|
  | RMS ≥ 20 Hz | −78.6 dBFS |
  | A-weighted | −79.8 dBFS |
  | Broadband floor without lines | −84.1 dBFS |
  | 1 kHz comb (sum) | −82.4 dBFS |
  | fs/8 line at 6000 Hz (block rate at block size 8) | −85.0 dBFS |
  | Mixing products n · f_SOF + m · 6000 Hz | −91.5 dBFS |

  The discrete lines make up about 70 % of the noise power. An ideal notch bank would gain ≈ 5.5 dB.
- **Compared with stock routing:** Once the patch gain is accounted for, the noise of `acdc/fx1d` equals `acdc/thru-usb-feedback` within ±0.2 dB. Two recordings in a row agree within 0.1 dB.
- **Known side effect:** Since the feedback fix, the mixing products are ≈ 4.8 dB higher than before (reproducible, still about 4–7 % of the noise). Given "Memory layout matters" above, the cause may be the changed code layout rather than the extra work in the USB interrupt.
- **Ground loops:** A loop appeared only in a measurement setup where the AC/DC was on a computer port and an audio interface fed from its outputs was on a hub. With both devices on the same hub it was gone. No USB isolator was needed.
- **Not addressed:**
  - the 1 kHz comb in recordings (it is in the ADC data while USB is active)
  - the LED PWM line at ≈ 2321 Hz: moved to 48 kHz with `v22.4.r-fx7a`, see "LEDs"
  - the rest of the fs/8 line (≈ 5 dB lower with `v22.4.r-fx3l`)

  Possible next experiments: block size 16, codec high-pass filter.

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
