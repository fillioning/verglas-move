# Clouds — Design Spec

> **Status:** Implemented (v1.0.0)
> **Plugin type:** audio_fx
> **Module ID:** `clouds`
> **Last updated:** 2026-03-11

---

## What it is

Mutable Instruments Clouds granular processor ported to Ableton Move as an audio
effect. Stereo audio input is captured into a recording buffer and replayed through
one of four processing modes: granular (overlapping grains with pitch shifting),
time-stretch (WSOLA), looping delay (with pitch shifting), or spectral (phase
vocoder). A feedback loop, diffuser, and reverb post-process the output. A
crossfade mix control blends dry input with the processed signal. The Freeze
function locks the buffer contents for infinite sustain effects.

## Sonic intent

**References:** Mutable Instruments Clouds (Eurorack granular processor by Emilie
Gillet), granular synthesis, spectral freezing, tape-loop textures, Boards of
Canada-style degraded audio, ambient washes.

**Philosophy:** Faithful port of the original DSP — same algorithms, same parameter
interactions, same sonic character. Adapted only where necessary for the Move
platform (sample rate, block size, buffer scaling). All four modes from the
original hardware are included.

**Not:** A creative reimagining or simplified version. Not a standalone synth — it
requires audio input. Not designed for zero-latency monitoring (granular processing
is inherently latent).

---

## DSP architecture

**Core algorithm:** `GranularProcessor` from `pichenettes/eurorack` (MIT license).
Mode-dependent: `GranularSamplePlayer` (overlapping grains), `WSOLASamplePlayer`
(time-stretch with cross-correlation), `LoopingSamplePlayer` (delay + pitch shift),
`PhaseVocoder` (STFT with `ShyFFT`).

**Voice architecture:** N/A (audio effect, not a synth)

**Signal flow:** Stereo int16 in → float conversion → mono mixdown (if mono quality)
→ feedback injection with HP filter → mode-dependent processing → diffuser (granular
modes) → pitch shifter (looper mode) → LP/HP filters (stretch/looper modes) →
feedback tap → Dattorro reverb → dry/wet crossfade (LUT-based) → SoftConvert →
stereo int16 out.

**Key adaptations from original:**
| What | Original (STM32) | Move |
|------|------------------|------|
| Sample rate | 32000 Hz | 44100 Hz |
| Block size | 32 samples | 128 samples |
| Large buffer | 118784 bytes | 163840 bytes |
| Small buffer | 65408 bytes | 90112 bytes |
| FFT | ARM CMSIS | shy_fft.h (portable) |
| Platform | STM32F4 bare-metal | aarch64 Linux (.so) |

**Known DSP challenges:**
- Spectral mode FFT at 44.1kHz may have different latency characteristics than 32kHz original
- `Prepare()` called in audio callback (originally in main loop) — heavy work only on mode/quality change, bounded cost
- Quality encoding: 2-bit field (bit 0=channels, bit 1=fidelity) — UI exposes only stereo variants (0 and 2)

---

## Parameters

### Knob-mapped (8 encoders, CC 71-78)
| Name | Type | Range | Default | Step | Notes |
|------|------|-------|---------|------|-------|
| Position | float | 0-1 | 0.5 | 0.01 | Grain start position in buffer |
| Size | float | 0-1 | 0.5 | 0.01 | Grain size / stretch window / delay time |
| Pitch | int | -24 to +24 | 0 | 1 | Semitone transposition |
| Density | float | 0-1 | 0.5 | 0.01 | Grain density (meta-param: <0.5 deterministic, >0.5 overlap) |
| Texture | float | 0-1 | 0.5 | 0.01 | Grain window shape / filter cutoff / spectral quantization |
| Feedback | float | 0-1 | 0.0 | 0.01 | Feedback amount (HP-filtered to prevent DC buildup) |
| Reverb | float | 0-1 | 0.0 | 0.01 | Post-reverb amount (Dattorro algorithm) |
| Mix | float | 0-1 | 0.5 | 0.01 | Dry/wet crossfade (LUT-based equal-power) |

### Jog-wheel only (navigable via jog menu)
| Name | Type | Range/Options | Default | Notes |
|------|------|---------------|---------|-------|
| Mode | menu | Granular / Stretch / Looper / Spectral | Granular | Playback algorithm |
| Freeze | toggle | OFF / ON | OFF | Locks recording buffer |
| Quality | menu | 16-bit / Lo-Fi | 16-bit | Lo-Fi = 8-bit mu-law + 2x downsampled |
| Spread | float | 0-1 | 0.5 | Stereo spread of grains |

---

## Hardware constraints (Move)

- Block size: 128 frames at 44100 Hz (~2.9ms per block)
- Audio: int16 stereo interleaved
- No FTZ on ARM — `-ffast-math` required to flush denormals
- No heap allocation in render path
- No `printf` or logging in render path
- Instance struct ~260KB (processor + two audio buffers) — allocated once via calloc
- Stack usage in render: 512 bytes (output buffer) — well within limits

---

## Open questions

- [ ] Pad interaction: `trigger`/`gate` fields in Parameters struct are set to false — could enable pad-triggered grain bursts in a future version
- [ ] Spectral mode latency: FFT hop size at 44.1kHz vs 32kHz may feel different — needs on-device testing
- [ ] Recording time: ~3.7s at 44.1kHz stereo 16-bit — may want to expose buffer size or allow mono-only for longer recording

---

## Design conversation reference

Port of Mutable Instruments Clouds from pichenettes/eurorack to Ableton Move via
Move-Anything framework. Follows the same approach as the working Rings port —
reusing stmlib, build infrastructure, and API patterns. All 4 modes implemented
in Phase 1.
