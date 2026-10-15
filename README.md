# root-plugins

Optional plugins for CERN ROOT. Each plugin is an independent top-level unit.
PhotoPeakFit runs directly in native ROOT with its existing fit controls.

## Build

A ROOT development installation with native GUI support, CMake, and a C++17
compiler are required. With `root-config` available in your environment:

```bash
make
```

This builds `build/lib/libPhotoPeakFit.dylib` on macOS, or
`build/lib/libPhotoPeakFit.so` on Linux, together with its ROOT dictionary and
`build/LoadPhotoPeakFit.C`. No application-specific host is required.

Equivalent CMake commands:

```bash
cmake -S . -B build
cmake --build build -j4
```

PhotoPeakFit also has an independent CMake entry:

```bash
cmake -S PhotoPeakFit -B build-photopeak
cmake --build build-photopeak -j4
```

Use `make BUILD_DIR=tests/custom-build` for a custom output directory. To select
a ROOT installation explicitly, pass `-DROOT_DIR=/path/to/root/lib/cmake/ROOT`
(or its build-tree configuration directory) when configuring with CMake.

## Run in ROOT

Start native ROOT and load the generated macro:

```bash
root -l build/LoadPhotoPeakFit.C
```

At the ROOT prompt, draw a one-dimensional histogram and start its fit session:

```cpp
TFile* file = TFile::Open("tests/hist_fragment62276.root");
TH1D* hist = file->Get<TH1D>("Array16/energy");
hist->GetXaxis()->SetRangeUser(300, 400);
hist->Draw();
PhotoPeakFit::Start(hist, gPad);
```

The data file above is a local test fixture and is not distributed with the
repository. Any directly drawn one-dimensional TH1 can be used instead.
For divided canvases, draw in the desired subpad and pass that pad to `Start`.

`PhotoPeakFit::Start()` uses the current pad and its selected or sole drawn
one-dimensional histogram. If multiple histograms are drawn, click the target
first or pass it explicitly. A pad and a target histogram can each belong to
only one active fit session. Starting the same session again raises its window;
switching its target requires stopping that session first. Failed API calls
return `false`; `PhotoPeakFit::LastError()` explains the failure.

The control window is non-modal. Its existing layout, parameter defaults,
background option menus, and three-row initial-peak pagination are retained.

| Interaction | Action |
|---|---|
| Left click / drag in the plot | Set / move the two red fit-range lines |
| Shift-left click | Add a peak, or remove an existing peak on release |
| Shift-left drag | Move a peak centroid |
| `f` | Fit |
| `n` / Clean | Clear fit drawings, range selections, and peak seeds |
| `w` / `q` | Rebin / undo rebin |
| Left / right arrow | Pan the histogram view |
| `b` | Display the PhotoPeak background |
| `o` | Unzoom |
| Delete in a peak row | Remove its seed and matching centroid marker |
| Previous / Next | Page through three initial-peak rows |
| Exit mode / close fit window | Discard the session |

ROOT-native canvas, axis, selection, drag, and context-menu behavior remains
active. PhotoPeakFit observes native events and routes them only to its owning
pad. Coordinate conversion supports logarithmic x axes.

Programmatic cleanup is also available:

```cpp
PhotoPeakFit::Clean(gPad);
PhotoPeakFit::Stop(gPad);
PhotoPeakFit::StopAll();
```

Exiting removes PhotoPeak drawings and releases its histogram clone and retained
fitter. Re-entry creates a fresh empty session. Removing the target drawing,
deleting the target or subpad, or closing the canvas invalidates the session.
Queued GUI commands are detached before the control window is deleted safely
by ROOT's event loop. Session operations run on ROOT's main GUI thread.

## Install

```bash
cmake --install build --prefix /path/to/photopeakfit
root -l /path/to/photopeakfit/LoadPhotoPeakFit.C
```

Keep the loader, `include/`, and library directory together. The installed
library, ROOT dictionary, and headers support moving the complete prefix.
Native `TCanvas` is supported; web canvases and `RCanvas` are outside this stage.

## Code structure

```text
PhotoPeakFit/
├── common/                  # Fit algorithms and configuration
├── session/                 # State, target validation, markers, and drawings
├── gui/                     # Existing ROOT control window and parameter rows
├── root/                    # Startup, event routing, dictionary, and loader
└── include/PhotoPeakFit/     # Public headers; detail/ contains ROOT receiver metadata
```

The ROOT entry coordinates sessions; GUI controls operate through a session;
sessions call the fitter. The fit algorithms do not depend on GUI controls or
event routing. Explicit fit calls can also run in ROOT batch mode.

## Tests

All test sources and generated test artifacts stay under `tests/`. Configure a
fresh test build and run the automated checks:

```bash
cmake -S . -B tests/root-build -DPHOTOPEAK_BUILD_TESTS=ON
cmake --build tests/root-build -j4
ctest --test-dir tests/root-build --output-on-failure
```

The tests cover numerical single/multiple peak fits, actual canvas signal
connections, target selection, independent pads, shortcuts, logarithmic
coordinates, drawing/object removal, and shutdown. When
`tests/hist_fragment62276.root` is present, a separate test fits its
`Array16/energy` and `Array17/energy` spectra read-only.

Run the native GUI checks from an interactive desktop session:

```bash
tests/root-build/photopeak-tests/photopeak_tests --gui
```

This additionally checks the existing control window's peak pagination, Delete,
Clean, and close actions. Test data, generated libraries, and logs stay ignored;
only focused test source files are versioned.
