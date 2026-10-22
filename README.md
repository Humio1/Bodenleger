# Bodenleger

A pad generator that lays a floor under your track, built from the track's own leftovers.
Offergeld's idea: because the pad is made only from sounds already in the project, it always sits in the mix.

## How it works

- **Return mode:** sits on a return track and listens to the last 7.5 seconds of whatever you send it.
  **Follow** recaptures every 8 seconds and crossfades; **Capture now** grabs a new snapshot on demand.
- **Samples mode:** drop up to four leftover sounds into the slots.
- The material is measured as a spectrum, split into tonal peaks and air, optionally filtered to the
  notes of the detected (or chosen) key, and resynthesised with random phases into seamless loops.
- **Drone** plays the resynthesised layers. **Wash** adds long grains of the source through tuned resonators and a long reverb.

Controls: Tonal, Noise, Drift, Color, Breath (sidechain depth), Mix.

**Character switches**

- **In key:** Scale (all scale notes), Chord (root, 3rd, 5th, 7th only), Shimmer (scale plus an octave-up copy).
- **Noise:** Air (smooth), Dust (vinyl-like crackle), Wind (sweeping resonant bands).
- **Breath:** Smooth follows the energy, Pump hits hard and lets go fast, Gate chops. At 100 % the pad is pulled down completely.
- **Listen:** Breath reacts to the plugin's input, or to the **sidechain input**. In Ableton, open the device's
  sidechain section and pick e.g. the kick track. The lamp next to *Sidechain* lights up while signal arrives.

**Modulation:** one LFO (Sine, Triangle, Square, Random) with free rate or tempo sync (8 bars to 1/16),
a depth control and switchable targets: Color, Tonal, Noise, Level, Pan. Modulated knobs show the range as a ring.

The window can be resized from the bottom-right corner; the size is remembered with the project.

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.9 is downloaded automatically.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target Bodenleger_VST3 Bodenleger_AU

Every push to `main` builds a universal macOS VST3 + AU on GitHub Actions (see the **Actions** tab, artifact
`Bodenleger-macOS`), runs the engine test and validates the plugin with pluginval. Pushing a tag like `v0.1.0`
publishes a release.

## Layout

    Source/Engine/   DSP and spectral engine, plain C++ (no JUCE), tested by tests/EngineTest.cpp
    Source/UI/       Bench look and components
    Source/          JUCE processor and editor
    Resources/fonts  Yellowtail, Oswald, Special Elite, Courier Prime (OFL / Apache 2.0)
