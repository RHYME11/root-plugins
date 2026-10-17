# root-plugins

Optional plugins for CERN ROOT. Each plugin is an independent top-level unit.
PhotoPeakFit runs directly in native ROOT with an English, non-modal fitting
control panel linked to the existing histogram canvas.
MatrixGate provides read-only TH2 projections through a separate ROOT library.
Its first stage supports X/Y gates, Separate/Sum/Union, and statistical errors;
background subtraction, the control window, and fitting integration are later stages.

## Build

A ROOT development installation with native GUI support, CMake, and a C++17
compiler are required. With `root-config` available in your environment:

```bash
make
```

This builds `build/lib/libPhotoPeakFit.dylib` and `build/lib/libMatrixGate.dylib`
on macOS (`.so` on Linux), together with their ROOT dictionaries and independent
`build/LoadPhotoPeakFit.C` and `build/LoadMatrixGate.C` loaders.
No application-specific host is required.

Equivalent CMake commands:

```bash
cmake -S . -B build
cmake --build build -j4
```

Each plugin also has an independent CMake entry:

```bash
cmake -S PhotoPeakFit -B build-photopeak
cmake --build build-photopeak -j4

cmake -S MatrixGate -B build-matrixgate
cmake --build build-matrixgate -j4
```

Use `make BUILD_DIR=tests/custom-build` for a custom output directory. To select
a ROOT installation explicitly, pass `-DROOT_DIR=/path/to/root/lib/cmake/ROOT`
(or its build-tree configuration directory) when configuring with CMake.

## Run PhotoPeakFit in ROOT

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
| Background | TSpectrum background used by Fit: scope, iterations, local range, options, Update and Disable |
| Results | Model, fit status, chi2/NDF, background treatment, fitted R/BETA/STEP/A/B/C, active WSCALE, per-peak values and errors, and total area |

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

The default tool is **Pick peaks**. Shift-click sets the range, and either range
line can be dragged in this same tool. **Browse** pauses fit picking while
preserving ROOT interaction.

| Interaction | Action |
|---|---|
| Left click in the plot | Add a peak, or select an existing nearby peak |
| Drag a peak marker | Move its centroid; clicking it again does not delete it |
| Shift-left click | Set successive fit-range bounds |
| Shift+E over the canvas | Open ROOT's native X-axis SetRangeUser dialog for this target |
| Drag a range line | Move that bound; crossing the other bound keeps the dragged line under the pointer |
| Delete selected / canvas Del | Delete the selected peak |
| Escape over the canvas | Cancel the gesture and enter Browse |
| Fit / canvas `f` or Enter | Validate pending edits and fit the same committed input |
| Clean / canvas `n` | Discard drafts, fit drawings, range selections, seeds, and results |
| Rebin / Undo / canvas `w` / `q` | Rebin / undo rebin |
| View buttons / left or right arrow | Pan the histogram view |
| Update background / canvas `b` | Recalculate the selected TSpectrum background |
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
Terminal and Results use the same macro-style report: initial peaks, requested
mode and ROOT options, effective background settings, relative constraints,
model/status, chi-square, grouped peak values and areas, and named raw fit
parameters. Fixed values, actual parameter limits, derived positions/widths,
and parameters inactive in the selected model are identified explicitly.
Long reports remain inside the Results viewport, including on macOS. Scroll
horizontally or vertically to read the full report; text selection is retained.

Multi-peak plots draw each colored component as the common fitted background plus
that peak's contribution. All components share the dashed black background,
including its polynomial and enabled smoothed-step terms. The red total curve
contains the background and all peaks. Reported peak areas and their errors
continue to describe the peak contribution alone.
Plot styling follows the local PhotoPeakFit macro: red total curves, black dashed
fitted backgrounds, distinct component colors, and a borderless transparent
legend at the upper left. The legend contains only a colored line and the fitted
centroid (three decimal places), including single-peak fits. Peak positions are
shown in the legend without additional rotated labels on the spectrum.
Legend height and text size adapt to the number of fitted peaks.

TSpectrum **participates in fitting** when Local or Global is selected. Selecting
either scope immediately draws a thick green dashed background estimate; changing its
iterations, options or local bounds updates that estimate. **Update background**
recalculates it. **Disable background** selects None, removes the estimate, and
disables subtraction for the next fit. None uses the ordinary fitted background.

Global estimates the entire spectrum regardless of zoom. Local uses its specified
range, expanding it to cover the fit range when needed; Results reports the
effective range. Fit subtracts the estimate from a detached histogram, retains
the original bin errors, and fits the remaining spectrum. The displayed total,
fitted background, and individual peaks all include the estimate again, so they
overlay the original spectrum. A/B/C and STEP describe the remaining fitted
background. The original histogram, errors, statistics, and zoom are preserved.

The estimate is treated as fixed; its uncertainty is not added to fit errors.
Background-subtracted fitting uses chi-square; ROOT likelihood option `L` is
rejected because the subtracted bins are not Poisson counts. Invalid background
settings stop the fit with a message instead of silently disabling subtraction.

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
root -l /path/to/photopeakfit/LoadMatrixGate.C
```

Keep the loader, `include/`, and library directory together. The installed
library, ROOT dictionary, and headers support moving the complete prefix.
Native `TCanvas` is supported; web canvases and `RCanvas` are outside this stage.

## MatrixGate projections

Load the independent plugin in native ROOT, including batch mode:

```bash
root -l build/LoadMatrixGate.C
```

The following example is self-contained at the ROOT prompt:

```cpp
TH2D matrix("example_matrix", "Example;Energy [keV];Time [ns]", 3, 0, 3, 2, 10, 30);
matrix.SetDirectory(nullptr);
matrix.SetBinContent(1, 1, 3);
matrix.SetBinContent(2, 1, 5);

MatrixGate::Config config;
config.gateAxis = MatrixGate::Axis::X;
config.combination = MatrixGate::Combination::Sum;
config.gates = {{1, "First", 0, 2, true}, {2, "Second", 1, 3, true}};
auto result = MatrixGate::Compute(matrix, config);
std::cout << result.Report();

// After checking result.success, draw the owned spectra.
auto* gateCanvas = new TCanvas("matrix_gate", "MatrixGate projections", 1000, 500);
gateCanvas->Divide(2, 1);
gateCanvas->cd(1);
result.projections[0].raw->Draw("hist");
gateCanvas->cd(2);
result.total->Draw("hist");
```

`result.total` is the total projection **on the gate axis**; each
`result.projections[i].raw` is the gated projection on the other axis. Setting
`gateAxis = MatrixGate::Axis::Y` reverses these roles. The example's first output
bin is 13, with variance 23: the overlapping source bin contributes twice to the
content and four times to the variance.

Each gate has `{id, name, low, high, enabled}`. IDs must be unique and nonzero,
including disabled gates. Enabled gates require finite coordinates satisfying
`axis minimum <= low < high <= axis maximum`. Coordinates use **[low, high)**:
the bin starting exactly at `high` is excluded. The selected interval expands to
complete intersecting bins; `GateBins` and `Report()` preserve both the requested
coordinates and actual edges, indices, and physical width. Variable bin edges,
axis titles, and bin labels are retained. Invalid enabled gates fail explicitly;
out-of-axis coordinates are not silently clipped. Disabled gates retain their
settings without contributing to the calculation. No enabled gates produces
only the total projection.

| Combination | Output |
|---|---|
| `Separate` (default) | One spectrum per enabled gate, retaining input order and IDs |
| `Sum` | One spectrum; shared bins contribute once for each selecting gate |
| `Union` | One spectrum; each selected source bin contributes once |

For source-bin weights `w`, contents use `sum(w * content)` and variances use
`sum(w * w * sourceVariance)`. This includes the covariance caused by selecting
the same bin repeatedly in Sum. Source-bin errors come from ROOT's `GetBinError`:
existing Sumw2 is retained; without it ROOT uses the square root of the absolute
content. Negative contents are preserved. Source cells are treated as independent;
physical correlations between different cells, including correlated symmetric
matrix filling, are not inferred. Separate output spectra may share cells and
are not necessarily statistically independent.

All flow bins are excluded by default. `config.flow` provides independent flags:

| Flag | Effect |
|---|---|
| `gateUnderflow` / `gateOverflow` | Include gate-axis flow bins in the total and in gates whose requested lower/upper endpoint touches the axis minimum/maximum |
| `resultUnderflow` / `resultOverflow` | Include result-axis flow bins in gated spectra and in the total projection's sums |

Recorded widths cover normal bins only. Sum widths include multiplicities;
Union widths count each normal bin once. Flow bins have no finite physical width.

`Compute` reads rectangular TH2 variants, including TH2D/F/I, without modifying
their contents, errors, statistics, directory ownership, or axis ranges. TH2Poly
and inputs with pending buffered fills are rejected; explicitly flush a pending
fill buffer before computing. Source zoom does not restrict the sums. Each call
recomputes from current bins and returns independent detached TH1D snapshots,
even after the input file closes. Keep the move-only `Result` alive while using
its spectra; destroy it or move it to transfer ownership. A failure returns
`success=false`, a diagnostic in `message`, and no partial spectra.

Only `background.model=None`, `background.selection=None`, default background
settings, and `errors=FixedCoefficients` are implemented in this stage. Other
declared background models and error policies are reserved and rejected. There
is no MatrixGate session/control window yet; drawing the returned spectra uses
ordinary ROOT canvases. MatrixGate does not link to PhotoPeakFit or GRSISort.

The calculation lives in `MatrixGate/common/`, the public types in
`MatrixGate/include/MatrixGate/`, and the ROOT entry, dictionary, and loader in
`MatrixGate/root/`. GUI, sessions, background calculations, export, and the
optional PhotoPeakFit connector will build on these layers.

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

All test sources and generated test artifacts stay under `tests/`, which the
current repository ignores in Git. These commands require the local test sources;
test sources and data fixtures are not included in a fresh checkout. Configure a
fresh test build and run the automated PhotoPeakFit checks:

```bash
cmake -S . -B tests/root-build -DPHOTOPEAK_BUILD_TESTS=ON
cmake --build tests/root-build -j4
ctest --test-dir tests/root-build --output-on-failure
```

The tests cover numerical single/multiple peak fits, actual canvas signal
connections, target selection, independent pads, shortcuts, logarithmic
coordinates, drawing/object removal, shutdown, stable peak IDs, stored mode preferences,
result snapshots and invalidation, local/global background estimation, and
background-offset component curves with unchanged numerical areas, fitted shared
parameters, centroid-only legends, and numbering resets after repeated Clean.
Single/multiple peak fits with Local/Global are compared with independently
subtracted spectra, including errors and areas. Tests also verify restored
backgrounds, preserved source data, invalid-setting errors and temporary-fitter
cleanup. When `tests/hist_fragment62276.root` is present, a separate test fits its
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
test source files follow the same local-only policy.
Long-report checks inspect the native viewport pixels through scrolling,
selection, resizing, replacement, clearing, and reopening.
Native GUI tests also open the actual Shift+E axis dialog, apply a range, cancel
it, and check that other pads and the existing fit stay unchanged.

The local MatrixGate numerical and interpreted-loader tests can be enabled with:

```bash
cmake -S . -B tests/matrixgate-build -DMATRIXGATE_BUILD_TESTS=ON -DPHOTOPEAK_BUILD_TESTS=ON
cmake --build tests/matrixgate-build -j4
ctest --test-dir tests/matrixgate-build --output-on-failure
```

MatrixGate checks cover non-square matrices in both directions, overlapping
Separate/Sum/Union gates, weighted and negative bins, variable edges, exact
boundaries, flow policies, stable IDs, disabled gates, input/global-state
preservation, independent snapshots, file closure, invalid requests, and explicit
rejection of background modes not yet implemented. Its loader smoke test calls
the public API through Cling and checks shared-bin statistical propagation.
