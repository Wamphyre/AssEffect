<p align="center">
  <img src="assets/ass-effect-logo.svg" alt="Ass Effect — Underground Lo-Fi Machine" width="760">
</p>

<h1 align="center">Ass Effect</h1>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.0.0-da4a2a" alt="Version 1.0.0">
  <img src="https://img.shields.io/badge/status-active-e89c30" alt="Status: active">
  <img src="https://img.shields.io/badge/platform-Linux%20only-f0cc65?logo=linux&amp;logoColor=191815" alt="Platform: Linux only">
  <img src="https://img.shields.io/badge/format-VST3-504a3e" alt="Format: VST3">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-BSD--3--Clause-6b6354" alt="License: BSD 3-Clause"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/wamphyre94078"><img src="https://img.shields.io/badge/Ko--fi-Support%20Wamphyre-ff5e5b?logo=kofi&amp;logoColor=white" alt="Support Wamphyre on Ko-fi"></a>
</p>

**Ass Effect**, created by **Wamphyre**, is a Linux-exclusive lo-fi VST3 effect for individual tracks, buses, and mastering. It models cassette tape, vinyl, and cheap or damaged equipment without forcing you to manage a chain of ten different plugins.

## Machines

- **90s Cassette**: level- and slew-dependent magnetic compression with memory, head bump, hiss, high-frequency loss, irregular wow/flutter, and soft-edged dropouts.
- **Worn Vinyl**: the limited bandwidth of a worn record, rumble, broadband surface dust, resonant clicks, and irregular slow wow.
- **4-Track Demo**: darker magnetic tracking, stronger level-dependent compression, and less stable narrow tape designed for guitars and raw demos.
- **Cellar Speaker**: cabinet and presence resonances, excursion-dependent cone damping, asymmetric overload, and aggressive degradation for vocals, drums, or parallel reamping.
- **Bitrot Sampler**: sample-and-hold processing and resolution reduction for cold digital degradation.

Noise and faults are generated directly by the DSP engine. No looping noise samples or external runtime assets are required.

## Controls

| Control | Function |
| --- | --- |
| Drive | Controls the level entering the machine's saturation stage. |
| Age | Controls bandwidth loss, magnetic sluggishness, and general ageing. |
| Wear | Controls tape dropouts or the density and scratch intensity of vinyl dust and clicks. |
| Wow / Flutter | Controls slow and fast speed instability. |
| Noise | Adds tape hiss or controls the level of vinyl surface noise, crackle, and rumble. |
| Grit | Drives continuous machine-dependent overload on the physical models; on Bitrot it controls sample-and-hold and bit reduction. |
| Tone | Compensates for or exaggerates the machine's darkness. |
| Width | Adjusts the stereo image from mono to 150%. |
| Mix | Controls parallel dry/wet processing. |
| Output | Sets the final output gain. |

Every control, including machine selection and bypass, is automatable and saved with the DAW session. Factory presets respond immediately to mouse or keyboard selection and are restored by name when the session is reopened; edited presets are restored as **CUSTOM / MODIFIED** with their exact parameter values.

## Included presets

Presets are grouped by intent in the interface: **Media**, **Instruments**, **Bus / Master**, and **Extreme**.

| Category | Preset | Intended use |
| --- | --- | --- |
| Media | **90s Walkman** | Balanced cassette colour, gentle hiss, and transport movement. |
| Media | **Worn Pressing** | Aged vinyl playback with surface noise, wear, and slow wow. |
| Instruments | **4-Track Guitar** | Saturated narrow tape for raw rhythm and lead guitars. |
| Instruments | **Buried Bass** | Dark, narrowed bass that remains controlled below the mix. |
| Instruments | **Cellar Drums** | Parallel small-speaker dirt for underground drum tones. |
| Instruments | **Crypt Vocals** | Dark, constrained, and unstable vocal degradation. |
| Instruments | **Rehearsal Mic** | Worn cassette capture with the feel of a rough room recording. |
| Instruments | **Frozen Sampler** | Cold sample-rate and bit-depth reduction for digital sources. |
| Bus / Master | **Tape Glue** | Subtle cassette cohesion with a conservative 20% wet mix. |
| Bus / Master | **Necro Master** | Narrow, dry bedroom 4-track damage inspired by the classic Darkthrone/Necrohell approach. |
| Bus / Master | **Raw Master** | Parallel cheap-speaker and fuzz haze inspired by classic Burzum production while retaining the dry low end. |
| Extreme | **Dust Dub** | Dense vinyl dust, frequent crackles, wide surface noise, and pronounced pitch movement. |
| Extreme | **Destroyed Room** | Aggressive cellar-speaker destruction for parallel processing. |

### Dust Dub

The revised **Dust Dub** preset uses the **Worn Vinyl** machine at 100% wet and is intentionally obvious even on dense material. It combines a continuous broadband dust bed, about 20 generated crackle events per second and channel, stronger wow/flutter, and an expanded stereo image. Its Output is reduced by 2.5 dB to retain peak headroom.

For a subtler result, lower **Wear** first to reduce event density, then lower **Noise** to move the complete vinyl artefact layer behind the source. Use **Mix** when the original transient and low-frequency content must remain intact.

The mastering presets use less degradation and a lower wet mix. When mastering, match the perceived level with **Output** before comparing processed and bypassed audio. For an extreme insert effect, raise **Mix** to 100%.

## Analogue modelling

- Cassette, vinyl, 4-track, and speaker `Grit` stages remain continuous at every setting. Sample-rate reduction and quantisation are exclusive to **Bitrot Sampler**.
- Non-linear processing is adaptively oversampled to run at approximately 176–192 kHz, reducing folded high-frequency harmonics while avoiding unnecessary work at high host sample rates.
- Cassette and 4-track magnetisation respond to both signal envelope and slew rate: loud material compresses progressively, while fast transitions encounter additional tracking loss and state-dependent saturation.
- Cellar Speaker combines low cabinet and mid-presence resonators with a cone model whose bandwidth and compression change with excursion; positive and negative overload use different curves.
- Wow and flutter combine transport drift, speed-dependent oscillation, and filtered random jitter instead of repeating a perfectly fixed LFO cycle. The delay line uses cubic interpolation to keep pitch movement smooth.
- Filter, noise-colour, dropout, DC-blocking, and magnetic-memory time constants are derived from the processing sample rate, so a machine keeps the same character at 44.1, 48, 96, or 192 kHz.
- Vinyl clicks excite short, randomised resonances rather than replaying the same exponential impulse shape.

## Real-time safety and performance

- A smooth safety ceiling controls internal wet-path overs before downsampling, followed by a narrow reconstruction guard that only catches filter overs above -0.54 dBFS. The **Output** control remains outside both stages, so deliberately adding output gain can still exceed 0 dBFS.
- Oversampling latency is reported to the host, and the dry path remains aligned through parallel mix and bypass.
- Delay lines, oversampling buffers, and dry-path compensation are allocated in `prepareToPlay`; the audio thread performs no heap allocations.

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
- `Source/FactoryPresets.h`: categorised and range-checked factory presets.
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

Ass Effect is released under the [BSD 3-Clause License](LICENSE).

## Support

If Ass Effect is useful in your music, you can support its development on [Ko-fi](https://ko-fi.com/wamphyre94078).
