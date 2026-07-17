<p align="center">
  <img src="assets/ass-effect-logo.svg" alt="Ass Effect — Underground Lo-Fi Machine" width="760">
</p>

<h1 align="center">Ass Effect</h1>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.0.0-da4a2a" alt="Version 1.0.0">
  <img src="https://img.shields.io/badge/status-active-e89c30" alt="Status: active">
  <img src="https://img.shields.io/badge/platform-Linux%20only-f0cc65?logo=linux&amp;logoColor=191815" alt="Platform: Linux only">
  <img src="https://img.shields.io/badge/format-VST3-504a3e" alt="Format: VST3">
</p>

**Ass Effect**, created by **Wamphyre**, is a Linux-exclusive lo-fi VST3 effect for individual tracks, buses, and mastering. It models cassette tape, vinyl, and cheap or damaged equipment without forcing you to manage a chain of ten different plugins.

## Machines

- **90s Cassette**: magnetic compression and saturation with memory, head bump, hiss, high-frequency loss, wow/flutter, and dropouts.
- **Worn Vinyl**: the limited bandwidth of a worn record, rumble, surface noise, dust, clicks, and slow wow.
- **4-Track Demo**: darker, more saturated, and less stable narrow tape designed for guitars and raw demos.
- **Cellar Speaker**: small-speaker band-pass filtering, asymmetric clipping, and aggressive degradation for vocals, drums, or parallel reamping.
- **Bitrot Sampler**: sample-and-hold processing and resolution reduction for cold digital degradation.

Noise and faults are generated directly by the DSP engine. No looping noise samples or external runtime assets are required.

## Controls

| Control | Function |
| --- | --- |
| Drive | Controls the level entering the machine's saturation stage. |
| Age | Controls bandwidth loss, magnetic sluggishness, and general ageing. |
| Wear | Controls tape dropouts or the density of vinyl dust and clicks. |
| Wow / Flutter | Controls slow and fast speed instability. |
| Noise | Adds tape hiss or vinyl surface noise and rumble. |
| Grit | Controls machine-dependent clipping, sample-and-hold, and bit reduction. |
| Tone | Compensates for or exaggerates the machine's darkness. |
| Width | Adjusts the stereo image from mono to 150%. |
| Mix | Controls parallel dry/wet processing. |
| Output | Sets the final output gain. |

Every control, including machine selection and bypass, is automatable and saved with the DAW session.

## Included presets

`90s Walkman`, `Worn Pressing`, `4-Track Guitar`, `Buried Bass`, `Cellar Drums`, `Crypt Vocals`, `Rehearsal Mic`, `Frozen Sampler`, `Necro Master`, `Raw Master`, and `Dust Dub`.

The mastering presets use less degradation and a lower wet mix. When mastering, match the perceived level with **Output** before comparing processed and bypassed audio. For an extreme insert effect, raise **Mix** to 100%.

## Building and packaging

Linux requires CMake, Git, a C++17 compiler, and the X11, Freetype, and Fontconfig development headers used by the JUCE interface. On Debian or Ubuntu:

```bash
sudo apt install build-essential cmake git pkg-config \
  libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxcursor-dev
```

Then run:

```bash
./build.sh
```

The script:

1. verifies the required tools and dependencies;
2. downloads JUCE 8.0.10 into `JUCE/` on the first run;
3. reuses the local JUCE checkout on subsequent builds;
4. configures and compiles a Release build;
5. copies the bundle to `releases/Ass Effect.vst3` and generates its SHA-256 checksum;
6. removes `build-release/` when it finishes, including after a failed build.

JUCE is kept as a cached source dependency, while the complete `releases/` directory is never deleted. Only the Ass Effect bundle is replaced. To preserve the temporary build tree for debugging:

```bash
KEEP_BUILD=1 JOBS=4 ./build.sh
```

CPU-specific optimisations are disabled by default to produce a portable binary. Enable them with `NATIVE_OPTIMIZATIONS=ON`.

## Installing on Linux

```bash
mkdir -p ~/.vst3
cp -a "releases/Ass Effect.vst3" ~/.vst3/
```

Restart the DAW or trigger a plugin rescan after installation.

## Project structure

- `Source/LoFiEngine.*`: real-time DSP with no memory allocation during audio processing.
- `Source/PluginProcessor.*`: parameters, presets, state handling, and audio input/output.
- `Source/PluginEditor.*`: resizable interface and level meters.
- `assets/ass-effect-logo.svg`: editable vector logo.
- `build.sh`: reproducible build, release packaging, and cleanup.

## Format and compatibility

- Platform: Linux only; the included release targets x86_64.
- Author and manufacturer: Wamphyre.
- Format: VST3 mono/stereo audio effect.
- Framework: JUCE 8.0.10.
- Toolchain: C++17 and CMake 3.22 or newer.
- No telemetry, embedded browser, or network dependency in the compiled plugin.

## License

Ass Effect is released under the [MIT License](LICENSE).
