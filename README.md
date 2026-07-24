# Multiband Convolver

A multiband convolution reverb VST3/Standalone plugin built with JUCE. Split incoming audio
across up to 8 frequency bands (click to add a band, drag its edges to resize it), then run
each band through its own independent convolution reverb loaded from a curated library of real
room/plate/spring impulse responses -- so, for example, the low end can sit in a tight room
while the highs bloom in a cathedral, all in one instance.

![Multiband Convolver GUI](docs/screenshot.png)

## Features

- **Draggable band editor** with a live spectrum analyzer background -- click empty space to add
  a band, drag an edge to resize it. Bands share crossover edges, so there are no gaps or
  overlaps between them.
- **Independent convolution per band** against a selectable impulse response.
- **Per-band macro controls**: Dry/Wet, Tone (tilt EQ), Fade In, Fade Out (decay/length control),
  Stretch, Feedback, Pre-Delay, Output, plus Bypass/Solo/Mute.
- **46 curated factory impulse responses** (real rooms, halls, garages, springs, plates -- see
  `Resources/IRs/CREDITS.md` for sourcing/licensing) across Residential, Commercial, Public,
  Historical, Outdoors, and Textures categories.
- **Custom IR support**: drop your own WAV/AIFF/FLAC files into the IR library's `Custom` folder
  (via the `...` menu -> "Open IR Library Folder") and they show up in the picker right alongside
  the factory library.
- **Presets**: save/recall full plugin states from the header dropdown.

## Installing

Grab the latest installer from the [Releases](../../releases) page (Windows only for now --
macOS support is planned separately). Run `Install Multiband Convolver.exe`, choose Standalone
and/or VST3 (both are checked by default), and confirm the VST3 install location if you use a
non-default plugin folder. The installer also places the factory IR library at
`Documents\MultibandConvolver\IRs`.

Once installed, add it in your DAW like any other VST3, or launch "Multiband Convolver" directly
as a standalone app to try it without a DAW.

## Using it

1. Click anywhere in the top spectrum display to add a band; drag an edge to resize it.
2. Click a band to load its macro controls into the rack below.
3. Pick an impulse response from the dropdown, then dial in Dry/Wet, Tone, Fade In/Out, Stretch,
   Feedback, Pre-Delay, and Output to taste.
4. Use Bypass/Solo/Mute per band to audition bands in isolation.
5. Save your settings as a preset from the header dropdown + floppy-disk icon.

## Building from source

Requires CMake 3.22+ and a C++20 compiler (Visual Studio 2022 Build Tools on Windows). JUCE is
fetched automatically via CMake's `FetchContent` on first configure.

```
cmake -B build
cmake --build build --config Release --target MultibandConvolver_VST3 MultibandConvolver_Standalone
```

Built artifacts land in `build/MultibandConvolver_artefacts/Release/`. To build the Windows
installer, install [Inno Setup](https://jrsoftware.org/isinfo.php) and run:

```
ISCC.exe Installer\MultibandConvolver.iss
```

There's also an offline DSP verification harness (`Tests/OfflineDspTest.cpp`, target
`OfflineDspTest`) that checks crossover flatness, feedback safety, and CPU budget at max band
count -- no audio device or DAW required to run it.

## Project layout

```
Source/               plugin C++ source
  DSP/                 crossover splitter, per-band chain, IR library/loading
  Params/              parameter layout, shared identifiers
  GUI/                 band editor, macro panel, custom LookAndFeel
Resources/
  IRs/                 factory impulse response library
  Textures/, Fonts/    GUI assets (see each folder's CREDITS.md for licensing)
Installer/             Inno Setup installer script
Tests/                 offline DSP verification harness
```

## Known limitations

- Windows only right now; macOS build is planned but not yet set up.
- Custom IR ordering in the picker is only stable while the custom file set itself doesn't
  change -- unlike the factory library, which has permanent fixed indices for preset/automation
  compatibility.
