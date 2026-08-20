# Multiband Convolver — Project Handoff

This doc is for anyone else picking up work on this project (with or without an AI pair-programmer)
— what it is, how it's built/tested/shipped, and the workflow conventions that have been
established so far. If you're using Claude or another AI assistant on this repo, point it at this
file first.

## What this is

Multiband Convolver is a VST3/Standalone convolution reverb plugin (JUCE/C++20/CMake, company
"Mirror Maze"). It splits incoming audio into up to 8 frequency bands via a Linkwitz-Riley
crossover and runs each band through its own independent convolution reverb, selectable from a
curated factory IR library plus user-droppable custom IRs.

- **Repo**: https://github.com/mirrorrmaze/multiband-convolver (public)
- **Local path** (on the primary dev machine): `D:\Claude Projects\MultibandConvolver`
- **Current version**: v0.6.0 (see [Releases](https://github.com/mirrorrmaze/multiband-convolver/releases))
- **Full feature list / usage / build-from-source instructions**: see `README.md` — this doc
  doesn't repeat what's already there, it covers process and history instead.

## Architecture, briefly

- **DSP**: `Source/DSP/CrossoverSplitter` (the LR4 cascade), `Source/DSP/BandChain` (one instance
  per band — pre-delay, `juce::dsp::Convolution`, tone tilt-EQ, feedback tap, dry/wet), backed by
  `IRLibrary` (catalog + file loading) and `IRProcessor` (pure IR shaping: stretch resample, fade
  in/out, growth-capped for CPU safety — see `IRProcessor.cpp`'s comments for why the cap has to
  anchor at the source's *natural* length rather than a flat ceiling).
- **Threading**: IR reshaping (disk read + resample + fade math) happens on a background thread
  (`IRReshapeWorker`), never the audio thread. The actual `loadImpulseResponse()` call still has
  to happen on the audio thread per JUCE's own threading contract — the worker only prepares the
  buffer, `BandChain::process()` picks it up and loads it. `IRWaveformView` (GUI) uses the same
  `IRProcessor::buildShapedIR` function on its *own* separate background thread, fully decoupled
  from the audio-facing one, so a busy display can never compete with real-time playback.
  Deadline-conscious debounce is used throughout — audio-thread reload requests debounce over
  ~200ms of samples, GUI-side ones over a ~120ms wall-clock `Timer` — so a rapid knob drag never
  fires more than one real reshape.
- **Parameters**: fixed-slot `AudioProcessorValueTreeState` layout — all 8 bands' worth of
  parameters are always registered (JUCE doesn't support safe runtime add/remove of
  `AudioProcessorParameter`s), gated by an `active`/`numBands` count instead.
- **GUI**: custom `LookAndFeel_V4` subclass (`LookAndFeelSaturnish`) — despite the name, it's the
  *current, kept* hardware-emulation theme (baked photo-texture metal knobs, chassis panel
  texture, Jost font, illuminated Bypass/Solo/Mute). A flat/minimal reskin modeled on a sibling
  project's GUI was prototyped and explicitly rejected — don't revisit that direction unless
  asked again.
- **Verification**: `Tests/OfflineDspTest.cpp` (target `OfflineDspTest`) — no audio device or DAW
  needed. Covers crossover flatness, solo frequency isolation, feedback safety, full IR-catalog
  load coverage, a Stretch-knob correctness invariant, band-settings-follow-split/merge, and an
  8-band CPU budget stress test. **Add a new assertion here for any new DSP behavior before
  considering a change done** — this has caught several real bugs (see History below).

## Build

```
cmake -B build
cmake --build build --config Release --target MultibandConvolver_VST3
cmake --build build --config Release --target MultibandConvolver_Standalone
cmake --build build --config Release --target OfflineDspTest
```

Build VST3 and Standalone as **separate** `cmake --build` invocations — passing both `--target`
names to one invocation only builds the last one listed (a real gotcha hit early on).

Windows installer: `ISCC.exe Installer\MultibandConvolver.iss` (needs Inno Setup, and a Release
build first — the script bundles `Resources\IRs\*` via a recursive wildcard, so new IRs never
need a script change). macOS build runs on GitHub Actions
(`.github/workflows/macos-build.yml`, `workflow_dispatch` — trigger via the Actions tab or
`gh workflow run macos-build.yml`), no local Mac needed; produces a `.pkg` as a downloadable
artifact.

## Versioning & releases

Bump the version in **three places together** before shipping anything meant to be detectable by
the in-app update checker:
- `CMakeLists.txt` — `project(MultibandConvolver VERSION X.Y.Z)`
- `Installer/MultibandConvolver.iss` — `MyAppVersion`
- `.github/workflows/macos-build.yml` — `pkgbuild --version`

Then publish an **actual new, higher-numbered GitHub Release** (`gh release create vX.Y.Z <win-exe>
<mac-pkg> --notes "..."`) — not a re-upload onto an old tag. `Source/UpdateChecker.cpp` compares
the compiled-in version against the *latest* release tag on launch (fails silently if there's no
network or nothing newer — never blocks startup or shows an error), so an already-installed copy
can only ever detect an update if there's somewhere higher for the tag to go.

## Distribution

GitHub Releases is the one and only distribution channel — `gh release create vX.Y.Z <win-exe>
<mac-pkg> --notes "..."` (see Versioning & releases above). No other handoff location.

## Git workflow

- **Branch before anything experimental or speculative** (a GUI redesign, an unproven approach) —
  makes it trivial to just `git branch -D` and walk away if it doesn't pan out, which has actually
  happened once already (a flat-theme reskin experiment).
- Regular fixes/features go straight to a feature branch → merge to `master` → push, once
  verified.
- **⚠️ Do not rewrite published history** (`git commit --amend` + force-push) now that more than
  one person works on this repo. That pattern was used a few times earlier in solo development to
  keep certain edits from showing as separate commits — it is **not safe** once a collaborator has
  their own clone, since a force-push after they've pulled will desync their branch and can lose
  their work. If something needs to not appear as a highly-visible standalone commit, just write a
  normal commit with a clear message instead.
- **This did happen once already, deliberately and for a different reason**: every commit's author
  email was originally a placeholder (`izaka@local`) that didn't match any verified GitHub email,
  so none of them linked to the account or showed up in GitHub's Contributors graph. Fixed with a
  one-time `git filter-branch --env-filter` rewriting author/committer email on all commits and
  tags, then `git push --force` on both — confirmed safe at the time because no collaborator clone
  existed yet. If you already have this repo cloned as of reading this, you're on the *post-rewrite*
  history; a fresh clone is the simplest way to be sure you match. Don't repeat this trick for
  routine reasons — it's a one-off fix for that specific bootstrapping problem, not a workflow.
- New commits should already carry a correctly-linking author identity going forward (`git config
  user.email` set to a verified GitHub-account email — a real address you've verified in
  github.com/settings/emails, or GitHub's own `<id>+<username>@users.noreply.github.com` format,
  which links without needing separate verification). Check `git config user.email` is actually
  set to something real before committing, rather than whatever placeholder a fresh machine
  defaults to.

## Known gotchas (worth knowing if you're using an AI assistant on this repo too)

- **Ableton locks the installed VST3** while it has the plugin loaded — a build's install-copy
  step fails until it's closed. It can also **skew CPU benchmark test results** (a live DAW
  session competing for CPU pushed the offline CPU-budget test's "realtime factor" from a normal
  ~0.85 up past the 1.0 fail threshold with byte-identical code) — check whether Ableton's running
  before trusting a CPU regression is real.
- **Screenshot-based / synthetic-click UI verification of the native app is unreliable in this
  environment** and hit two real incidents: once grabbing an unrelated foreground window (the
  user's live Ableton session) due to a `MainWindowHandle` timing race right after launch, and
  once — even with window-title verification passing — capturing a completely unrelated window
  (a video call) because window-handle-based screen capture doesn't reliably respect z-order here.
  **Just build the change, verify what's verifiable non-interactively (tests, code review, layout
  math), and ask a human to look at the actual GUI.** Don't sink time into automated visual
  verification for this project.
- `cmake --build` with multiple `--target` names in one invocation silently only builds the last
  one — always issue separate invocations.

## Recent history (condensed)

Roughly chronological, most recent first, as of v0.6.0:

- **v0.6.0**: The update checker gained an actual popup (Download / Skip This Version) on top of
  the existing passive header-menu tint + menu item. "Skip This Version" persists to a small local
  file so it won't nag again for that exact release, but still prompts normally for anything newer.
- **v0.5.0**: IR picker is grouped by category (Residential/Commercial/Public/Historical/Outdoors/
  Textures/Custom) with a bold heading per category instead of one flat list of 95+ entries,
  matching the picker layout in the sibling GGrid project's own convolver. Categories are grouped
  for *display only*, built by walking a fixed category order and pulling in matching entries
  regardless of where they sit in the catalog -- the factory catalog's entries were appended across
  several historical batches, so a naive "heading whenever the category changes" approach produced
  the same category (e.g. Textures) as three separate headings instead of one consolidated section.
  Item IDs stay exactly `catalogIndex + 1` throughout, so saved presets/automation are unaffected.
- **v0.4.0**: Frequency reference grid in the band editor (Pro-Q-style log-spaced Hz gridlines);
  click/drag a crossover edge to see its exact Hz in a popup, double-click to type a precise value.
- **v0.3.0**: IR waveform view in the macro panel — shows the selected band's impulse response at
  a *fixed* scale (only changes with Stretch/IR selection, not Fade), Fade In baked into the
  visible taper, Fade Out shown as a dimmed cut region rather than a rescale (an earlier version
  literally rescaled the waveform on Fade Out, which read as the sample visually "stretching" —
  reworked to match how a real hardware/software convolver's IR view behaves). Also fixed a
  genuine rendering artifact where a quiet IR's tail showed visible banding instead of a flat line
  (near-zero float jitter across hundreds of columns confusing the path rasterizer — fixed by
  flooring negligible amplitude to a hard zero before building the display path).
- **v0.2.0**: In-app update checker (background GitHub Releases check, silent-fail, opens the
  release page from the header menu). Grew the factory IR library from 46 to 95 impulse responses
  (more rooms across every category, plus plates/springs/found-object textures). Fixed a real
  Stretch-knob bug where an earlier CPU-safety cap could clamp a stretched IR's length *below* its
  own natural length, flipping the resample direction — Stretch would sometimes speed an IR up
  instead of slowing it down, and audibly do nothing across part of its range. Also fixed the
  underlying CPU-spike bug that motivated the cap in the first place (Stretch had no absolute
  ceiling, so a long factory IR at 4x could build a 40+ second convolution kernel).
- **Earlier**: full hardware-emulation GUI redesign (baked-texture metal knobs, chassis panel,
  Jost font, illuminated Bypass/Solo/Mute), Windows installer with component selection, custom IR
  support (drop-your-own-files + path-based persistence so picker-position drift across
  preset/project reloads doesn't silently point at the wrong file), a background-thread IR
  reshape fix for CPU spikes/dropouts while dragging Stretch, a fix for per-band settings not
  following the band's *identity* (not just its numeric index) when bands are split/merged, and
  the macOS build pipeline via GitHub Actions (including a non-obvious YAML gotcha: an unquoted
  colon in a step name breaks the parser).
