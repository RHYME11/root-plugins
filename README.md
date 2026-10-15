# root-plugins

Optional plugins for CERN ROOT. Each plugin is an independent top-level unit.
PhotoPeakFit runs directly in native ROOT with an English, non-modal fitting
control panel linked to the existing histogram canvas.

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

## Control panel

The English control panel follows the compact toolbar and canvas-picking workflow
of [jROOT Tools UltraFitEnv](https://github.com/jsmallcombe/jRootAnalysisTools/tree/v4.1).
It uses ROOT-native widgets and observes the existing canvas after ROOT handles
its native input. The session remains locked to its original histogram and pad.

The target, active tool, fit range, view actions, and Fit/Clean/Exit buttons remain
visible. Four pages organize the settings:

| Page | Controls |
|---|---|
| Peaks | Scrollable peak list, Add/Delete, selected centroid/height/FWHM, Auto/HighStat/LowStat, relative position and relative FWHM |
| Shape | R, BETA, STEP, A, B, C, WSCALE, and advanced ROOT fit options |
| Background | TSpectrum preview scope, iterations, local range, and background options |
| Results | Model, fit status, chi2/NDF, fitted R/BETA/STEP/A/B/C, active WSCALE, per-peak values and errors, and total area |

Every parameter has a Value and a mutually exclusive Free/Fixed/Limited mode.
Limited mode expands Lower/Upper fields. Inactive settings retain their stored
values: for example, LowStat applies its parameter restrictions to the fit without
replacing the user's HighStat settings. Relative position uses the lowest-centroid
seed as its reference; relative FWHM uses the stored seed widths and WSCALE.
Its checkbox stays checked when individual FWHM editors are disabled. With fewer
than two peaks, relative settings are inactive and their checkboxes retain the
stored preference.

Peak identities remain stable when their centroids are edited, dragged, or sorted.
Selecting a peak in the list highlights its canvas marker and opens its editor.
The list replaces the former three-peak pagination and handles larger peak sets
without constructing another parameter editor for each peak.

## Canvas interaction

The default tool is **Pick peaks**. **Set range** offers range selection without
a modifier key. **Browse** pauses fit picking while preserving ROOT interaction.

| Interaction | Action |
|---|---|
| Left click in the plot | Add a peak, or select an existing nearby peak |
| Drag a peak marker | Move its centroid; clicking it again does not delete it |
| Shift-left click | Set successive fit-range bounds |
| Set range tool + left click | Set successive fit-range bounds |
| Drag a range line | Move that bound; crossing the other bound keeps the dragged line under the pointer |
| Delete selected / canvas Del | Delete the selected peak |
| Escape over the canvas | Cancel the gesture and enter Browse |
| Fit / canvas `f` or Enter | Validate pending edits and fit the same committed input |
| Clean / canvas `n` | Discard drafts, fit drawings, range selections, seeds, and results |
| Rebin / Undo / canvas `w` / `q` | Rebin / undo rebin |
| View buttons / left or right arrow | Pan the histogram view |
| Preview / canvas `b` | Update or clear the TSpectrum preview according to its scope |
| Unzoom / canvas `o` | Unzoom the histogram axes |
| Exit / close control window | Discard and release the session |

Enter or leaving a numeric field validates and commits its edits. Apply, changing
selection/page, and canvas operations use the same submission path. In an input
field, Enter commits and Del retains its normal text-editing meaning. Invalid
values stay visible with an English diagnostic; dependent operations stop until
the values are corrected. Clean and Exit can discard invalid drafts.

The Results page keeps the last fit and its input snapshot separately from the
initial seeds. Input or binning changes mark that result out of date and remove
its old fit drawings; selecting a peak or changing the view does not invalidate
it. Fit failure, unapplied edits, and a current successful result have distinct
status messages. Results do not overwrite the initial parameter values.
The panel repaints the latest result when fitting finishes, without another
click or tab change. Both the panel and terminal report fitted shared parameters
and their uncertainties; these are separate from the stored initial settings.

Multi-peak plots draw each colored component as the common fitted background plus
that peak's contribution. All components share the dashed black background,
including its polynomial and enabled smoothed-step terms. The red total curve
contains the background and all peaks. Reported peak areas and their errors
continue to describe the peak contribution alone.
The legend contains only a colored line and the fitted centroid (three decimal
places) for each peak, including single-peak fits.

TSpectrum is a **preview**, separate from the A/B/C background used by the fitting
model. None clears the preview, Local uses its specified range, and Global uses
the entire spectrum regardless of the current zoom. Iteration count and all
background option groups apply to the preview. Its calculation uses a detached
copy and preserves the source histogram's axis range.

ROOT-native canvas, axis, object-selection, cursor, drag, and context-menu behavior
remains active. Events and shortcuts are scoped to the owning pad; coordinate
conversion supports logarithmic axes. Ordinary canvas drags on unrelated objects
do not add peaks or change fit bounds.

Programmatic cleanup is also available:

```cpp
PhotoPeakFit::Clean(gPad);
PhotoPeakFit::Stop(gPad);
PhotoPeakFit::StopAll();
```

Clean retains fit settings and the current binning, and restarts peak numbering
at 1 for the next selection. Exiting removes PhotoPeak
drawings and releases its histogram clone and retained fitter. Re-entry creates a
fresh empty session. Removing the target drawing, deleting the target or subpad,
or closing the canvas invalidates the session. Queued GUI commands are detached
before the control window is deleted safely by ROOT's event loop. Session
operations run on ROOT's main GUI thread.

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
├── gui/                     # English tabbed control panel and parameter editors
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
coordinates, drawing/object removal, shutdown, stable peak IDs, stored mode preferences,
result snapshots and invalidation, local/global background previews, and
background-offset component curves with unchanged numerical areas, fitted shared
parameters, centroid-only legends, and numbering resets after repeated Clean. When
`tests/hist_fragment62276.root` is present, a separate test fits its
`Array16/energy` and `Array17/energy` spectra read-only.

Run the native GUI checks from an interactive desktop session:

```bash
tests/root-build/photopeak-tests/photopeak_tests --gui
```

This additionally checks the English control window with 25 peaks, stable
selection after sorting, invalid draft retention, mode switching, Fit button/key
equivalence, Delete, Clean, close actions, and scrollbar visibility on first
display and reopening, relative-width checkbox/editor consistency, and immediate
result updates after fitting and refitting. Test data, generated libraries, and logs stay ignored;
only focused test source files are versioned.
