# Verglas

Mutable Instruments Clouds granular processor as audio FX for Ableton Move, built for the [Move Everything](https://github.com/charlesvestal/move-everything) framework.

**Verglas** is a faithful port of Emilie Gillet's [Clouds](https://mutable-instruments.net/modules/clouds/) — four processing modes (granular, time-stretch, looping delay, spectral), feedback, reverb, freeze, and quality degradation. Extended with output HPF/LPF filters and a tape-style soft limiter.

## Install

Available from the Move Everything Module Store on your device, or manually:

```bash
./scripts/build.sh              # or: docker build -t verglas-builder . && docker cp $(docker create verglas-builder):/build/modules/verglas ./modules/verglas
./scripts/install.sh            # deploys to Move via SSH
```

## Parameters

| Knob | Parameter | Range |
|------|-----------|-------|
| 1 | Position | 0–100% |
| 2 | Size | 0–100% |
| 3 | Pitch | -24 to +24 st |
| 4 | Density | 0–100% |
| 5 | Texture | 0–100% |
| 6 | Feedback | 0–100% |
| 7 | Reverb | 0–100% |
| 8 | Mix | 0–100% |

Additional parameters (jog menu): Mode, Freeze, Quality, Spread, HPF, LPF, Limiter, Pre Gain, Post Gain.

See [MANUAL.md](MANUAL.md) for full documentation.

## Credits

- **DSP**: Emilie Gillet / [Mutable Instruments](https://github.com/pichenettes/eurorack) (MIT License)
- **Soft clipper**: Inspired by [Chowdhury DSP](https://github.com/Chowdhury-DSP) and [Airwindows](https://github.com/airwindows/airwindows)
- **Move port**: fillioning
- **Framework**: [Move Everything](https://github.com/charlesvestal/move-everything)

## License

DSP code: MIT (Emilie Gillet). Port and wrapper: MIT.
