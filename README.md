# DrawSynth

A VST3/CLAP/Standalone instrument built with JUCE: you draw on a canvas and the drawing becomes music.
X = time (one loop, synced to host tempo), Y = pitch (quantized to a key/scale). Draw a continuous
line for a legato melodic line; lift the pen and draw again for a new phrase.

This is the v1 build: drawing canvas, key/scale quantization (major/minor + all seven modes as a
stretch), tempo sync, one voice, a built-in 3-oscillator ADSR synth, MIDI export, and MIDI-out
passthrough. Multi-voice colors, WAV export, and a full undo history are deferred (see the bottom
of this file) — the data model already has the seams for them, described below.

**Build status**: every claim in this file about compiling was actually checked. VST3, Standalone,
and CLAP were all built from this exact source in a clean Linux container (JUCE 8.0.7, GCC 13); the
VST3 exports the correct `GetPluginFactory`/`ModuleEntry`/`ModuleExit` symbols and reports itself as
an Instrument/Synth, and the CLAP binary exports `clap_entry`. The pure music-logic core (scale
quantization, stroke-to-note-timeline conversion) has an independent test suite that passes 24/24
checks with zero JUCE dependency. What hasn't been checked: actually loading the plugin in a DAW and
listening to it (no audio device / GUI in the build environment), and Windows/macOS builds - see
"Building the Windows installer" below for exactly what is and isn't verified about `DrawSynth-Setup.exe`.

## Building

You need CMake 3.22+, a C++17 compiler, and (Linux only) the usual JUCE dependencies:

```bash
# Linux only - install once:
sudo apt-get install -y cmake build-essential libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
```

Then, from this directory:

```bash
cmake -B build
cmake --build build --target DrawSynth_VST3 --config Release -j
```

CMake fetches JUCE 8.0.7 automatically on first configure (no manual JUCE install needed). If you
already have a local JUCE checkout and would rather not re-download it, point CMake at it instead:

```bash
cmake -B build -DJUCE_PATH=/path/to/your/JUCE
```

Other useful targets: `DrawSynth_Standalone` (a runnable app, no DAW required - the fastest way to
audition it) and `DrawSynth` (builds the shared code without any format wrapper, useful for a quick
syntax check). Installed plugin locations follow JUCE's normal per-OS conventions (e.g.
`~/.vst3/DrawSynth.vst3` on Linux, `~/Library/Audio/Plug-Ins/VST3` on macOS,
`C:\Program Files\Common Files\VST3` on Windows).

### Building the CLAP target

JUCE does not build CLAP plugins natively (still true as of 8.0.7) - CLAP support comes from the
community [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) project, which
CMakeLists.txt wires up as an **opt-in** target so the default build never depends on it:

```bash
git submodule add https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
git submodule update --init --recursive
cmake -B build -DDRAWSYNTH_BUILD_CLAP=ON
cmake --build build --target DrawSynth_CLAP --config Release -j
```

(If this isn't a git repository yet, `git init` first, or just clone the extension straight into
`libs/clap-juce-extensions` with `--recursive` instead of using a submodule.)

### Building the Windows installer (Setup.exe)

DrawSynth itself builds natively with MSVC on Windows (JUCE's officially supported Windows compiler)
- the same `CMakeLists.txt` above works unchanged there. What's added here is packaging that build
into a double-click installer with [NSIS](https://nsis.sourceforge.io/), via
`installer/DrawSynth-Setup.nsi`.

**Fastest path - no Windows machine needed:** push this repo to GitHub, then open the Actions tab and
run "Build Windows Installer" (or just push to `main`). It builds DrawSynth with real MSVC on a free
GitHub-hosted Windows runner, packages it with NSIS, and uploads `DrawSynth-Setup.exe` as a
downloadable artifact on the finished run - see `.github/workflows/build-installer.yml`.

**Locally, on your own Windows machine:** install [Visual Studio](https://visualstudio.microsoft.com/)
(the free Community edition, with the "Desktop development with C++" workload) and
[NSIS](https://nsis.sourceforge.io/Download), then from this directory in a Developer Command Prompt:

```bat
cmake -B build -A x64
cmake --build build --config Release --target DrawSynth_VST3 DrawSynth_Standalone
"C:\Program Files (x86)\NSIS\makensis.exe" installer\DrawSynth-Setup.nsi
```

Either way, `installer\DrawSynth-Setup.exe` installs the VST3 into the shared `Common Files\VST3`
folder every VST3 host (including FL Studio) scans automatically, plus an optional standalone `.exe`,
with a proper Windows uninstaller and Add/Remove Programs entry.

**What's verified vs. not.** The installer *script* is genuinely tested: `makensis` compiles it into a
valid Windows PE executable with the correct sections, install paths, and uninstaller, checked in this
same environment against placeholder files laid out exactly like JUCE's real build output
(`build/DrawSynth_artefacts/Release/VST3/...`). What is **not** verified is the actual Windows-compiled
`DrawSynth.vst3`/`.exe` this packages - that only happens the first time the GitHub Actions workflow
(or your own Windows build) actually runs, since this environment is Linux-only with no way to run
Windows binaries or MSVC. A Linux-side cross-compiled Windows binary isn't an option either: JUCE
explicitly refuses to build under MinGW (`#error "MinGW is not supported"` in
`juce_core/system/juce_TargetPlatform.h`) - MSVC is required, which is exactly what both paths above use.

### Running the logic tests

The music-theory core (`Source/Music`, `Source/Canvas`) has zero JUCE dependency by design, so it
compiles and runs in about a second with a plain compiler - no CMake, no JUCE checkout needed:

```bash
g++ -std=c++17 -o logic_test tests/logic_test.cpp \
  Source/Music/Scale.cpp Source/Music/ScaleQuantizer.cpp \
  Source/Canvas/StrokeModel.cpp Source/Canvas/NoteTimelineBuilder.cpp
./logic_test
```

## Controls

**Grid shape** (top-left row) - changing any of these re-quantizes the existing drawing on the spot,
nothing needs to be redrawn:
- **Key** - the 12 chromatic roots.
- **Scale** - Major, Natural Minor, and all five other modes (Dorian, Phrygian, Lydian, Mixolydian,
  Locrian) plus Harmonic and Melodic Minor.
- **Octaves** - how many octaves the Y axis spans (1-4).
- **Oct. Shift** - moves that window up or down without changing its size.
- **Quantize** - horizontal grid resolution, in notes per beat (1/2/3-triplet/4).
- **Loop** - loop length in bars (1/2/4/8). Beats-per-bar follows the host's time signature when one
  is reported, otherwise assumes 4/4.

**Sound shape** (top-right row):
- **Sync To Host** - on by default (tempo follows the host); off enables the **BPM** slider for a
  manual tempo, useful for auditioning outside a DAW.
- **Osc** - Sine / Saw / Triangle (naive, non-bandlimited oscillators - adequate for v1, a clear spot
  for a future polyBLEP upgrade if you want less aliasing on high notes).
- **A/D/S/R** - the built-in synth's envelope.
- **Level** - output gain.

**Transport** (bottom bar):
- **Play** - starts/stops the plugin's own loop, independent of the host transport, so you can
  audition a drawing without pressing play in your DAW. When the host *is* playing and Sync To Host
  is on, playback locks to the host's beat clock instead for sample-accurate alignment with the rest
  of your project.
- **Record** - while armed and playing, incoming MIDI notes (from your DAW or a MIDI keyboard) are
  captured onto the canvas as new strokes, using the same code path as drawing with the mouse.
- **Clear** - erases the whole canvas. **Undo** - removes the most recently drawn stroke (single-step;
  see "Deferred" below for the full undo *history* this isn't).
- **Export MIDI...** - writes the current pattern, one loop cycle, to a `.mid` file.

There's no color/voice picker in this build - see "Deferred" below.

## How drawing becomes music

1. **`StrokeModel`** stores exactly what you drew: a list of strokes, each a list of
   `(timeBeats, normalizedY, pressure)` points. Nothing here is quantized - Y is a raw 0..1 fraction
   of the canvas height, not a MIDI note.
2. **`ScaleQuantizer`** is the only thing that knows how a Y position maps to a MIDI note, given the
   current key/scale/octave window. Its inverse is used to place recorded MIDI notes back onto the
   canvas.
3. **`NoteTimelineBuilder`** combines the two: it lays a time grid over the loop (resolution = the
   Quantize setting), samples each stroke's interpolated Y at each grid cell, quantizes it, and
   run-length-encodes consecutive same-pitch cells into a `NoteEvent`. A pitch change with no gap
   inside one continuous stroke is flagged `legatoFromPrevious` instead of getting a hard cut - that's
   the "continuous line = legato glide" behaviour. This whole rebuild re-runs from scratch on every
   edit or grid-setting change, which is what makes quantization retroactive: strokes are never
   touched, only re-read.
4. **`NoteTimeline`** (the output of step 3) is the single interchange format. `SynthEngine` consumes
   it directly each audio block, splitting the block at every note boundary for sample-accurate
   timing, and emits the same information as outgoing MIDI in lockstep. `MidiExporter` consumes the
   *same* structure independently to write a `.mid` file. Neither has to know anything about strokes,
   pixels, or canvases - which is also exactly the seam a future offline WAV render would reuse (call
   `SynthEngine::renderBlock` in a loop against a timeline instead of live audio callbacks).

Legato is implemented as a true monophonic glide: `SynthVoice` skips the envelope's attack stage and
smoothly ramps oscillator frequency (over a fixed ~50ms) when a note is legato-linked to the one
before it, rather than hard-retriggering. For MIDI-out and file export, this is represented using the
standard mono-legato convention (new note-on emitted just before the old note-off), which
legato-aware synths read as a glide and everything else just reads as a clean, gapless handover.

### A documented thread-safety trade-off

The rebuilt `NoteTimeline` is published from the message thread to the audio thread via a
`SpinLock`-guarded `shared_ptr` swap (`PluginProcessor::publishTimeline` / the lock in
`processBlock`). This is realtime-safe *in practice* (rebuilds only happen on user edits, not every
block, and timelines are small) but not textbook wait-free: in the rare case the audio thread ends up
holding the last reference to a stale timeline when its local copy is destroyed, that small
deallocation happens on the audio thread. If you need harder real-time guarantees, replace this with
a lock-free triple buffer or an epoch-based reclamation scheme; the seam is entirely contained in
`getCurrentTimelineForDisplay()` / `publishTimeline()` / `rebuildTimelineIfNeeded()` in
`PluginProcessor.cpp`.

### MIDI-out to route into FL Studio (or any host)

DrawSynth declares itself an instrument that also produces MIDI output (`NEEDS_MIDI_OUTPUT`, a
standard VST3 capability for instruments). Whether a given host exposes a *plugin's own* MIDI output
for routing to another track varies by host and is generally a per-plugin setting in the host's
mixer/wrapper UI rather than something DrawSynth controls - worth checking your host's plugin wrapper
or MIDI-routing settings if you want to send DrawSynth's output to another instrument. The MIDI file
export (Export MIDI... button) works everywhere regardless of host MIDI-routing support, since it's
just a normal file.

## Deferred (kept out of this v1 pass on purpose)

- **Multi-voice colors.** The data model is already color-ready - `StrokePoint`/`Stroke`/`NoteEvent`
  all carry a `colorIndex` (always `0` here), and `SynthEngine` already tags outgoing MIDI channel as
  `colorIndex + 1`. Adding it means: a color picker in the UI, `SynthEngine` owning one `SynthVoice`
  per color instead of one, and `NoteTimelineBuilder`'s composited-cell array keying on `(cell, color)`
  instead of just `cell` so different colors don't paint over each other.
- **WAV export.** Not implemented, but the architecture note above is what makes it "trivial" when it
  is: repeatedly call `SynthEngine::renderBlock` against a `NoteTimeline` into an offline buffer, then
  write it with `juce::WavAudioFormat`.
- **Full undo history.** A single-step undo (last stroke) is implemented since it's essentially free
  on top of the stroke list; a multi-level undo/redo stack was left out.
- **Custom skins / themes.** The UI uses a single fixed dark theme.
