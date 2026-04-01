# VOID CHANT — Occult Vocal Synthesizer Plugin

> VST3 / AU · JUCE 7 · C++17 · Universal Binary (arm64 + x86_64)

## Overview

VOID CHANT is a synthesizer plugin built around five ritual parameters that shape a dark, choral sound. The UI mirrors the plugin's occult aesthetic: a stone-carved magic circle that glows in response to MIDI velocity and ADSR envelope state, surrounded by five stone-sphere knobs with neon-blue arc meters.

## Parameters

| Parameter | Range | Effect |
|---|---|---|
| **MURMUR** | 0–100% | Continuous vowel morph A→E→I→O→U via 3-band parallel formant filter (F1/F2/F3) |
| **RITUAL** | 0–100% | Unison density 1–8 voices with micro-delay offsets (1–5 ms) and stereo spread |
| **POSSESS** | 0–100% | Pitch drift via random-walk LFO smoothed by `LinearSmoothedValue` |
| **SACRIFICE** | 0–100% | Master volume per voice |
| **DEPTH** | 0–100% | Cathedral reverb wet mix (`juce::dsp::Reverb`, room=0.97) |

## Building

### Prerequisites
- CMake 3.22+
- Xcode 14+ (macOS) or Visual Studio 2022 (Windows)
- Git

### macOS (Universal Binary)
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build build --config Release --parallel
```

### Windows (x64)
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

## CI/CD

GitHub Actions automatically builds VST3 (Windows) and VST3+AU (macOS Universal) on every push to `main`. Tagged releases (`v*`) trigger a GitHub Release with all artifacts attached.

### Mac "File Damaged" Prevention

Three measures are applied:

1. **Universal Binary** — `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` ensures native execution on all Mac architectures without Rosetta translation issues.
2. **Hardened Runtime + Entitlements** — `codesign --options runtime` with `VoidChant.entitlements` (includes `cs.allow-unsigned-executable-memory`) prevents DAW-level memory execution rejection.
3. **ditto packaging** — `ditto -c -k --sequesterRsrc` preserves macOS extended attributes and symlinks, avoiding the metadata corruption that plain `zip` causes.

## License

All source code © VOID CHANT Audio. All rights reserved.
