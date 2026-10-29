# Parallax64 LCXL Synth

by Flix

An 8-voice subtractive synthesizer for Windows, built with [JUCE](https://juce.com) and made to be played from a **Novation Launch Control XL MK3**. Runs as a **VST3 plug-in** and as a **standalone application** (WASAPI/ASIO).

## Features

- 8 voices, two anti-aliased oscillators (PolyBLEP) plus sub oscillator and noise, optional hard sync
- Unison (detuned, stereo-spread), Mono/Legato/Glide
- Five filter modes: 12 dB lowpass, 24 dB lowpass, highpass, bandpass and a ladder filter (LAD)
- Two ADSR envelopes (amplitude and filter), LFO to pitch, filter or pulse width, with tempo sync
- Arpeggiator with 8-step pattern, direction, octave range, gate and host sync
- Effects: distortion, delay (five stereo modes, tempo sync) and reverb
- 128 factory presets in eight categories (bass, lead, pluck, keys, pad, arp, drums, FX), matched in loudness
- User presets: SAVE stores the current sound under a suggested name (UserPreset1, UserPreset2, ...)
- Five skins
- MIDI learn (right-click a control); learned mappings are saved with the state. The default control mapping follows the factory template of the Launch Control XL (encoders CC 13-36, faders CC 5-12, buttons CC 37-52)

## Download

Windows builds (VST3 and standalone) are on the [Releases page](https://github.com/maxpower30303/Parallax64-LCXL-Synth/releases/latest).

- **Standalone:** unzip and run `Parallax64-LCXL-Synth.exe`.
- **VST3:** copy the folder `Parallax64-LCXL-Synth.vst3` to `C:\Program Files\Common Files\VST3` and rescan plug-ins in your DAW.

The executable is unsigned, so Windows SmartScreen may show a warning. The SHA256 checksum of each zip is listed in the release notes.

## Build (Windows)

Requirements: Visual Studio (C++20 toolchain) and CMake 3.22 or newer. JUCE 9.0.2 is downloaded automatically.

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Parallax64_LCXL_Synth_Standalone
cmake --build build --config Release --target Parallax64_LCXL_Synth_VST3
```

Use the generator name of your installed Visual Studio version. To build against a local JUCE checkout instead of downloading it, pass `-DJUCE_DIR=<path to JUCE>` (or set the `JUCE_DIR` environment variable).

Results are placed in `build/Parallax64_LCXL_Synth_artefacts/Release/`. ASIO support uses the headers bundled with JUCE; no separate ASIO SDK is required.

## License

Parallax64 LCXL Synth is free software, licensed under the **GNU Affero General Public License v3.0** (`AGPL-3.0-only`), see [LICENSE.txt](LICENSE.txt). This is the licence option under which JUCE 9 is used here.

Third-party components and their licences are listed in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt), which must be shipped together with binary releases. Anyone who distributes this software or a modified version must pass on the source code under the same licence.

The software is provided "as is", without warranty of any kind. Developed with the assistance of artificial intelligence (AI).

### Name

The code is covered by the licence above. The names "Parallax64" and "Parallax64 LCXL Synth" identify the original project; please do not use them as the name of a modified or derived product.

"VST" is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries. "ASIO" is a trademark and software of Steinberg Media Technologies GmbH. "Launch Control XL" and "Novation" are trademarks of Novation; this project is not affiliated with Novation. All other names are the property of their respective owners; no affiliation with or endorsement by them is implied.

## Contributing

Issues and pull requests are welcome. By submitting a contribution you confirm that you wrote it (or have the right to submit it) and agree that it is licensed under `AGPL-3.0-only` like the rest of the project.

(c) 2026 Flix
