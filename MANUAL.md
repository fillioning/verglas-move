# Verglas — User Manual

> Mutable Instruments Clouds granular processor for Ableton Move

Verglas is a port of Emilie Gillet's legendary [Mutable Instruments Clouds](https://mutable-instruments.net/modules/clouds/) Eurorack module for Ableton Move. It captures incoming stereo audio into a buffer and replays it through four distinct processing modes — from shimmering granular textures to frozen spectral landscapes. Additional output processing (HPF, LPF, soft limiter) extends the original with practical tools for taming the signal.

---

## Getting Started

1. Add **Verglas** to your signal chain as an audio effect
2. Send audio into it — drums, synths, samples, anything
3. Turn up **Mix** (knob 8) to hear the wet signal
4. Explore the four modes via the jog wheel menu

Verglas needs audio input to work. Feed it something and start turning knobs.

---

## Knobs (1–8)

| Knob | Parameter | What it does |
|------|-----------|-------------|
| 1 | **Position** | Where in the buffer to read. At 0% you hear the oldest audio, at 100% you hear the newest. Sweep it to scrub through the buffer like a tape head. |
| 2 | **Size** | Grain size in Granular mode, stretch window in Stretch mode, delay time in Looper mode, spectral warp in Spectral mode. Small = glitchy, large = smooth. |
| 3 | **Pitch** | Transposition in semitones (-24 to +24). Independent of time — pitch up without speeding up. |
| 4 | **Density** | How many grains overlap. Below 50%: periodic triggering (sparse, rhythmic). Above 50%: random clouds of grains (dense, textural). At 50%: minimal single grains. Default: 100%. |
| 5 | **Texture** | Grain window shape in Granular mode. Filter cutoff in Stretch/Looper modes. Spectral quantization in Spectral mode. Always interesting to sweep. |
| 6 | **Feedback** | Routes processed output back into the input. Low values add depth, high values build up into self-oscillating drones. Has a built-in highpass filter to prevent DC buildup. |
| 7 | **Reverb** | Post-processing reverb (Dattorro algorithm). Adds space and diffusion to the output. Not fed back into the feedback loop. |
| 8 | **Mix** | Dry/wet crossfade. 0% = clean input, 100% = fully processed, 50% = equal blend. Uses equal-power crossfade for smooth transitions. |

---

## Jog Wheel Menu

Click the jog wheel to enter the parameter menu. Rotate to navigate, click to edit, rotate to change value, click to confirm.

### Mode

Selects the processing algorithm:

- **Granular** — Classic granular synthesis. Audio is sliced into overlapping grains that can be pitch-shifted, time-stretched, and scattered. The signature Clouds sound: ethereal, diffused, sparkling.

- **Stretch** — Time-stretching via WSOLA (Waveform Similarity Overlap-Add). Plays audio back at the original pitch regardless of position changes. Smooth, continuous, tape-like. Good for frozen drones and slow-motion effects.

- **Looper** — Looping delay with pitch shifting. Acts like a delay line where you can independently shift the pitch of the delayed signal. Good for harmonizer effects and rhythmic delays.

- **Spectral** — Phase vocoder (FFT-based). Freezes and manipulates the spectral content of the audio. Creates metallic, robotic, crystalline textures. The most extreme and alien-sounding mode.

### Freeze

Locks the recording buffer. No new audio is captured — the buffer contents are preserved indefinitely. Combined with Position and Size, you can explore a frozen moment of sound forever. Toggle ON/OFF.

### Quality

- **16-bit** — Full quality stereo processing
- **Lo-Fi** — 8-bit mu-law encoding with 2x downsampling. Adds grit, aliasing, and a lo-fi character reminiscent of early samplers.

### Spread

Stereo spread of the grain cloud. At 0% the output is mono, at 100% grains are panned across the full stereo field.

---

## Output Processing

After the Clouds engine, Verglas adds three optional processors to shape the final output:

### HPF (Highpass Filter)

One-pole 6dB/octave highpass filter. Range: OFF (0 Hz) to 1000 Hz. Removes low-end rumble from the processed signal. Useful when feedback builds up bass energy.

### LPF (Lowpass Filter)

One-pole 6dB/octave lowpass filter. Range: 1000 Hz to OFF (20000 Hz). Tames harsh highs from granular artifacts or spectral processing. Softens the output.

### Low-Shelf EQ

Biquad low-shelf filter applied before HPF/LPF. Boosts low frequencies for added warmth and weight.

- **Low Boost** (knob 6) — Boost amount, 0 to +6 dB (default: 0 = off). No CPU cost when at 0.
- **Low Freq** (knob 7) — Shelf frequency, 30–400 Hz (default: 100 Hz). Sets where the boost begins.
- **Low Q** (knob 8) — Shelf Q factor, 0.1–4.0 (default: 0.7). Lower = gentle slope, higher = resonant bump at the shelf frequency.

### Limiter

Tape-style soft clipper at the end of the chain. Tames peaks without hard clipping.

- **Limiter** — ON/OFF toggle (default: OFF)
- **Pre Gain** — Input drive, -6 dB to +6 dB. Push into the soft clipper for saturation.
- **Post Gain** — Output level after clipping, -6 dB to +6 dB. Compensate for volume changes.

The soft clipper uses a cubic polynomial for gentle saturation and a sine waveshaper for harder peaks — inspired by tape saturation characteristics.

---

## Tips & Recipes

### Ambient Pad from Drums
- Mode: **Granular**
- Position: 50%, Size: 75%, Pitch: +12, Density: 80%
- Texture: 60%, Feedback: 30%, Reverb: 50%, Mix: 80%
- Feed in a drum loop and let it dissolve into shimmer

### Frozen Drone
- Mode: **Spectral** or **Stretch**
- Play audio, then enable **Freeze**
- Sweep Position slowly to explore the frozen sound
- Add Feedback and Reverb for infinite sustain

### Lo-Fi Tape Loop
- Mode: **Looper**
- Quality: **Lo-Fi**
- Size: 40-60%, Pitch: 0, Feedback: 60%
- Texture controls the filter — sweep for wah-like effects

### Glitch Machine
- Mode: **Granular**
- Size: very small (5-15%), Density: 20-40%
- Position: automate or sweep quickly
- Pitch: random jumps between -12 and +12

### Spectral Freeze + Pitch
- Mode: **Spectral**
- Freeze: ON
- Sweep Pitch in semitones for harmonizer/robot effects
- Texture adds quantization for metallic, tuned resonances

---

## Parameter Reference

| Parameter | Range | Default | Step | Knob | Notes |
|-----------|-------|---------|------|------|-------|
| Position | 0–100% | 50% | 1% | 1 | Buffer read position |
| Size | 0–100% | 50% | 1% | 2 | Grain/window/delay size |
| Pitch | -24 to +24 st | 0 | 1 st | 3 | Semitone transposition |
| Density | 0–100% | 100% | 1% | 4 | Grain overlap/trigger rate |
| Texture | 0–100% | 50% | 1% | 5 | Window shape / filter / quantization |
| Feedback | 0–100% | 0% | 1% | 6 | Feedback amount |
| Reverb | 0–100% | 0% | 1% | 7 | Post-reverb wet amount |
| Mix | 0–100% | 50% | 1% | 8 | Dry/wet crossfade |
| Mode | Granular / Stretch / Looper / Spectral | Granular | — | Jog | Processing algorithm |
| Freeze | OFF / ON | OFF | — | Jog | Lock recording buffer |
| Quality | 16-bit / Lo-Fi | 16-bit | — | Jog | Audio resolution |
| Spread | 0–100% | 100% | 1% | Jog | Stereo spread |
| HPF | OFF–1000 Hz | OFF | 10 Hz | Jog | Highpass filter cutoff |
| LPF | 1000–20000 Hz | OFF | 100 Hz | Jog | Lowpass filter cutoff |
| Limiter | OFF / ON | OFF | — | Jog | Tape soft clipper |
| Pre Gain | -6 to +6 dB | 0 dB | 0.5 dB | Jog | Limiter input drive |
| Post Gain | -6 to +6 dB | 0 dB | 0.5 dB | Jog | Limiter output level |
| Low Boost | 0 to +6 dB | 0 dB | 0.5 dB | Filters 6 | Low-shelf boost amount |
| Low Freq | 30–400 Hz | 100 Hz | 10 Hz | Filters 7 | Low-shelf frequency |
| Low Q | 0.1–4.0 | 0.7 | 0.1 | Filters 8 | Low-shelf Q factor |

---

## Changelog

### v1.1.0

- **Fixed** volume drop at Mix 0% (Clouds' equal-power crossfade attenuated dry signal by -3dB)
- **Fixed** Spectral mode being significantly louder than other modes
- **Fixed** pitch-dependent volume loss (wet signal gain compensation)
- **Added** texture knob smoothing (100ms one-pole, eliminates zipper noise)
- **Added** low-shelf EQ with 3 parameters (Low Boost, Low Freq, Low Q) on Filters page knobs 6-7-8

### v1.0.0

- Initial release — all four Clouds modes, output HPF/LPF, tape limiter

---

## Credits

- **DSP**: Emilie Gillet / [Mutable Instruments](https://github.com/pichenettes/eurorack) (MIT License)
- **Soft clipper**: Inspired by [Chowdhury DSP](https://github.com/Chowdhury-DSP) and [Airwindows](https://github.com/airwindows/airwindows)
- **Move port**: fillioning
- **Framework**: [Schwung](https://github.com/charlesvestal/move-everything)
