# Clouds — Claude Code context

## What this is
Mutable Instruments Clouds granular processor as audio FX for Ableton Move via Move-Anything.

Plugin type: audio_fx
Module ID: `clouds`
API: `audio_fx_api_v2_t`, entry: `move_audio_fx_init_v2`
Language: C++ (DSP) with C linkage exports

Read `design-spec.md` for full design intent. This file is the compressed version.

---

## Sonic intent
Faithful port of Emilie Gillet's Clouds (pichenettes/eurorack, MIT). Four modes:
granular, time-stretch (WSOLA), looping delay, and spectral (phase vocoder).
Processes stereo audio input into textural, granular, frozen, and spectral effects.
Not a standalone synth — requires audio in. Not a simplified version — all four
original modes are included.

---

## DSP architecture
Stereo int16 in → float conversion → mono mixdown (if mono quality) → feedback
injection with HP filter → mode-dependent processing (GranularSamplePlayer /
WSOLASamplePlayer / LoopingSamplePlayer / PhaseVocoder) → diffuser (granular modes)
→ pitch shifter (looper mode) → LP/HP filters (stretch/looper) → feedback tap →
Dattorro reverb → dry/wet crossfade (LUT-based equal-power) → SoftConvert →
stereo int16 out.

Source: `src/clouds/dsp/` adapted from pichenettes/eurorack. Key changes:
- sample_rate: 32000→44100 in `granular_processor.h`
- kMaxBlockSize: 32→128 in `frame.h`
- Large buffer: 118784→163840 bytes (scaled for 44.1k, same recording time)
- Small buffer: 65408→90112 bytes
- Spectral mode uses shy_fft.h (not ARM CMSIS)
- Quality parameter: `set_quality(quality * 2)` — only stereo variants exposed
  (bit 0=channels, bit 1=fidelity; we always pass bit 0=0 for stereo)

---

## Parameters (8 encoders + 4 jog-only)
| # | Key           | Name     | Type   | Range    | Default | CC  | Step |
|---|---------------|----------|--------|----------|---------|-----|------|
| 1 | position      | Position | float  | 0-1      | 0.5     | 71  | 0.01 |
| 2 | size          | Size     | float  | 0-1      | 0.5     | 72  | 0.01 |
| 3 | pitch         | Pitch    | int    | -24-+24  | 0       | 73  | 1    |
| 4 | density       | Density  | float  | 0-1      | 0.5     | 74  | 0.01 |
| 5 | texture       | Texture  | float  | 0-1      | 0.5     | 75  | 0.01 |
| 6 | feedback      | Feedback | float  | 0-1      | 0       | 76  | 0.01 |
| 7 | reverb        | Reverb   | float  | 0-1      | 0       | 77  | 0.01 |
| 8 | dry_wet       | Mix      | float  | 0-1      | 0.5     | 78  | 0.01 |
| J | mode          | Mode     | menu   | 0-3      | 0       | jog | 1    |
| J | freeze        | Freeze   | toggle | 0-1      | 0       | jog | 1    |
| J | quality       | Quality  | menu   | 0-1      | 0       | jog | 1    |
| J | stereo_spread | Spread   | float  | 0-1      | 0.5     | jog | 0.01 |

CCs are **relative** (1-63=increment, 65-127=decrement). Pitch knob uses
accelerated delta for fast semitone jumps. Mode wraps around 0-3.

Quality mapping: UI value 0→`set_quality(0)` (stereo 16-bit),
UI value 1→`set_quality(2)` (stereo lo-fi / 8-bit mu-law + 2x downsampled).

---

## Critical implementation notes (never violate)
- **Quality encoding**: `set_quality(quality * 2)` — the 2-bit field is (bit1=lofi, bit0=mono). We always keep bit0=0 for stereo.
- **Denormals**: compile with `-ffast-math` — ARM has no FTZ
- **No heap in render**: all buffers in instance struct, allocated once via calloc
- **No printf in render**: use `g_host->log()` only in init/create
- **calloc + Init()**: instance allocated with calloc, then `processor.Init()` called. Do not add constructors with side effects.
- **Prepare() before Process()**: must call `Prepare()` every block — handles mode changes, buffer reallocation, vocoder buffering
- **Separate output buffer**: `Process(input, output, size)` reads from input at the end for dry/wet mix — do not modify input before Process completes
- **Relative CC accumulation**: `val = (cc_data < 64) ? cc_data : cc_data - 128` then accumulate
- **Knob overlay**: `knob_N_adjust/name/value` pattern is REQUIRED — Shadow UI calls DSP directly

---

## Jog wheel menu (ui_chain.js)
- Jog rotate: navigate 12 parameters
- Jog click: enter edit mode → rotate adjusts value → click confirms
- Display: mode name, quality, freeze status, current param name + value
- Knobs 1-8 (CC 71-78) → direct control of first 8 params

---

## Move hardware constraints (never violate)
- Block size: 128 frames at 44100 Hz (~2.9ms)
- Audio: int16 stereo interleaved
- No FTZ on ARM — `-ffast-math` required
- No heap allocation in render path
- No printf / logging in render path
- Files on device must be owned by `ableton:users`
- Instance struct ~260KB (processor + 163840 + 90112 byte buffers)

---

## API-specific constraints (audio_fx)
- API: `audio_fx_api_v2_t`, entry symbol: `move_audio_fx_init_v2`
- `process_block`: in-place int16 stereo, 128 frames
- `host_api_v1_t` must have ALL 13 fields — missing any crashes
- `audio_fx_api_v2_t` field order: process_block BEFORE set_param/get_param/on_midi
- Capabilities: `chainable: true, audio_in: true`
- Install path: `/data/UserData/move-anything/modules/audio_fx/clouds/`

---

## Build
```bash
# Docker (preferred)
docker build -t clouds-builder .
docker cp $(docker create clouds-builder):/build/modules/clouds ./modules/

# Deploy
./scripts/install.sh [move-ip]
```

Compiler flags: `-std=c++17 -O2 -fPIC -ffast-math -fno-exceptions -fno-rtti -DTEST`

Source files (9): clouds_move.cpp, granular_processor.cc, correlator.cc, mu_law.cc,
resources.cc, phase_vocoder.cc, stft.cc, frame_transformation.cc, units.cc

---

## Repo map
- `src/clouds_move.cpp` — Move API wrapper + params + state serialization
- `src/clouds/dsp/` — adapted Clouds DSP (granular_processor, sample players, correlator)
- `src/clouds/dsp/fx/` — reverb (Dattorro), diffuser, pitch shifter, fx_engine
- `src/clouds/dsp/pvoc/` — phase vocoder, STFT, frame transformation (spectral mode)
- `src/clouds/resources.cc` — lookup tables (xfade, sine window, grain size, quantized pitch)
- `src/clouds/drivers/debug_pin.h` — stub (no debug pins on Move)
- `src/stmlib/` — adapted stmlib (from Rings port + shy_fft.h, buffer_allocator.h, atan.h, rsqrt.h)
- `module.json` — module metadata
- `ui_chain.js` — Shadow UI (jog menu + knob handling)
- `design-spec.md` — authoritative design record
- `scripts/build.sh` — Docker ARM64 cross-compile
- `scripts/install.sh` — deploy via SSH + fix ownership
