# root-plugins

Optional plugins for CERN ROOT. Each plugin is an independent top-level unit.
PhotoPeakFit runs directly in native ROOT with an English, non-modal fitting
control panel linked to the existing histogram canvas.
MatrixGate provides native ROOT TH2 gating, background subtraction, and interactive
dual-canvas analysis through a separate ROOT library.
It supports X/Y gates, Separate/Sum/Union, template subtraction, and the RadWare
standard 2D background model. The control window and fitting integration are later stages.

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

`PhotoPeakFit::Start()` opens one shared control window. Left-click another pad
in the attached canvas to activate it; that first click only switches targets.
Hovering and right-clicking do not switch the active pad. A red outline identifies
the active pad, and keyboard shortcuts follow that target. **Fit layer** selects
a directly drawn 1D histogram when several share a pad. Returning to a pad restores
its last selected layer. Each pad/layer keeps its own settings, peak list, results,
and unfinished editor text, including invalid input. Native ROOT interactions
remain available.

**Overwrite from previous target** copies the previous target's complete fitting
configuration and editor draft onto the selected layer, replacing its prior
PhotoPeakFit settings and clearing its fit results. The source, histogram data,
other layers, and native drawings remain unchanged. The source is named above the
button. **Undo overwrite** restores that layer's previous state and fit drawings.
Copied settings outside the target's range remain visible and must be corrected
before fitting; they are never silently clipped. Overwrite does not run a fit.
New layers start from default settings and their own visible range. Rebinning,
source-snapshot refresh, and Clean discard the pending overwrite undo. State is
kept for the live ROOT session; it is not saved across ROOT restarts. A histogram
cannot simultaneously own fitting sessions in different pads. Failed API calls
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
| Left click in another pad | Activate its last fit layer without adding a peak |
| Left click in the active plot | Add a peak, or select an existing nearby peak |
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

Load the independent plugin in native ROOT:

```bash
root -l build/LoadMatrixGate.C
```

For an existing `TH2 *gg`, open the native control window with:

```cpp
MatrixGate::Start(gg);
// After changing the matrix contents externally:
MatrixGate::Update(gg);
// Close one window, or all MatrixGate windows:
MatrixGate::Stop(gg);
MatrixGate::StopAll();
```

`Start` and `Update` return `false` on failure; `MatrixGate::LastError()` gives the
diagnostic. `Start(gg, config)` accepts initial settings. Calling `Start` again for
the same matrix raises its existing window. Each source matrix has an independent
window, and deleting the source, closing its owning file, or closing either
embedded canvas releases the session. The source is borrowed and must remain
alive; MatrixGate neither owns it nor changes its bin contents or axis ranges.
The GUI requires a native desktop ROOT session. `Compute` remains available in
batch mode.

The window shows the gate-axis total projection on the left, above the gate
sliders, and the gated result on the right. `Raw` (signal-gated S), `Scaled BG` (the correction alpha * H),
and `Net` (S - alpha * H) can be shown separately
or overlaid; the estimated one-dimensional background curve is shown on the
total projection when the selected method supplies one. Separate results are
available through the `Shown result` selector. The `Report` tab describes the
last successful numerical snapshot, including normalization and uncertainties.

In `Pick range` mode, click two edges on the total projection to edit the selected
gate; with no selection this creates a signal gate. Double-click to move its
center, or drag a dashed boundary. `Browse` (also Escape) pauses plugin picking
while ROOT's native axis, zoom, object, and context-menu interactions remain
available. Ordinary canvas drags are not interpreted as two-edge picks. Delete
removes the selected gate and Enter applies the editors. These shortcuts apply
only to the total-projection canvas. The result canvas and unrelated canvases retain
their native ROOT behavior.

Left-click a canvas to select it and give it keyboard focus. Both canvases
support these shortcuts; the receiving native canvas window,
rather than the globally current pad, selects the target. Text-entry and unrelated
canvas keystrokes do not operate MatrixGate plots. The shortcuts also work in Browse.

| Key in the focused canvas | Action |
|---|---|
| Left / Right arrow | Pan that plot by 10% of its visible X span |
| `o` | Unzoom that plot's X/Y axes and restore automatic Y scaling |
| `w` | Rebin that plot's display by another factor of two |
| `q` | Undo one rebin step on that plot |
| Shift+E | Open ROOT's native X-axis `SetRangeUser` dialog for that plot |
| `f` | Open PhotoPeakFit for that plot in a separate fitting canvas |

One shared toolbar contains Rebin, Undo, the current factor, and PhotoPeakFit.
The red frame and `Active plot` selector identify its target. Only a left click
on a canvas switches the target automatically; hovering, entering/leaving a
canvas, and right/middle clicks do not switch it. The selector can also choose
and focus a target explicitly. Returning from an editor, left-click the desired
canvas to resume its shortcuts.
Keyboard events still operate only the canvas receiving the native event, so
typing in an editor or using an unrelated canvas cannot operate the red-framed plot.
Display binning is independent for the two plots and for each gate-axis direction,
and survives recalculation. Rebin rebuilds from the immutable original spectra,
retains a final incomplete group, sums counts and statistical variances, and
preserves the common subtraction-scale covariance in BG/net errors. Undo
restores the finer display without altering the source TH2, calculation spectra,
or source-bin gate selection. Result overlays share a Y scale computed from
the currently visible X interval, including negative counts. Explicit native Y
limits are retained until unzoomed with `o` or ROOT's axis controls.

For fitting, select Total or Result and click `PhotoPeakFit (f)` (or left-click
that canvas and press `f`). For Result, `Fit layer` explicitly selects Raw, Scaled BG,
or Net, independently of the visible overlay checkboxes and for the currently
shown gate result. Total always fits the total projection. PhotoPeakFit is
loaded on demand from beside MatrixGate or ROOT's dynamic library search path;
build both plugins together with `make`. MatrixGate calculations and gating
remain usable when this optional component is absent.

The fitting canvas owns a detached snapshot at the current display binning and
initial visible X range. Use the existing PhotoPeakFit controls: left-click one
or more peaks, Shift-click range edges, then press `f` inside the **fitting
canvas** or click Fit. Its peak selection, range, model, results, and additional
rebinning are independent of MatrixGate and other fit windows. Fits consume the
snapshot's per-bin errors; the fitter does not use MatrixGate's full cross-bin
covariance matrix.

Gate/background edits mark gated fit snapshots `OUT OF DATE` without changing
their old counts or peak seeds. Total fits stay current across gate edits;
changed total data/errors/binning invalidate them. Fitting a stale snapshot is
blocked. Reopen PhotoPeakFit from the corresponding MatrixGate target to refresh
the snapshot, retain valid seeds/settings, and refit explicitly. Separate gates
and result layers can have independent fit windows. Deleting a gate purges gated
fit windows; switching the gate axis purges all of this session's fit windows.
Closing MatrixGate or deleting its TH2 also closes its fitting canvases and
controls, while unrelated PhotoPeakFit sessions continue running.

The `Gates` tab provides one scrollable slider row per gate, with a matching
stable color, an enabled flag, and a Delete button. Select a row to edit its
name and Low/High or Center/Width numerically; the actual selected bin edges
appear below. Signal boundaries use dashed lines and BG boundaries use long
dashes. Visible Signal/BG numbering is consecutive within each kind; stable
internal identities need not be consecutive and do not retain deleted objects.
Deleting a gate removes its controls, markers, cached settings, and old gated
results, and immediately recalculates even with Auto Update off. A failed
recalculation leaves the affected result empty with a diagnostic. Deleting the
last Manual BG selects Template=None.

Slider limits follow the **visible left-hand X-axis range**, in physical units.
Every gate, including a disabled gate, stays inside that range. A zoom change
preserves any gate that still fits unchanged. Only an out-of-view gate is mapped
proportionally from the old visible interval to the new one, following the
GRSISort slider interaction. Zoom changes that preserve all gates do not
invalidate numerical results. New gates, numeric edits, marker drags, and
center moves respect the same limits. Double-click in Pick mode on the left
plot to move the selected center with its width fixed; near an edge the whole
gate stops at the visible limit.

Manual Relative numeric bounds are offsets from signal centers. The sliders
show physical coordinates for the shown signal. All relative instances must
fit inside the zoom, so their common allowed offset range can be narrower.
Switching absolute/relative templates converts coordinates; an incompatible
multi-signal arrangement produces a visible validation error. X and Y retain
independent gates and zoom settings. `Separate`, `Sum`, and `Union` use the same
calculation as `Compute`.

The `Background` tab exposes template/RadWare models, fraction estimator,
uncertainty convention, TSpectrum options, fit/normalization ranges, overlap
policy, and explicit flow-bin flags. RadWare requires the symmetric-gamma-gamma
confirmation and compatible settings described below. Imported background
vectors can be supplied in the initial config; `Use automatic curve` clears
them. Template and RadWare settings are retained separately when changing models.
`Template=None` disables subtraction while retaining inactive preferences.
Adding a BG gate activates Manual (or retains Manual Relative); in RadWare mode,
select Template subtraction first. Overlapping BG windows form a union under
the selected overlap policy, rather than double-counting shared source bins.
For direct gate-width subtraction, select Model=Template subtraction,
Template=Manual, and Fraction=Width ratio. Add a signal and use `+ Background`
once for each background interval. Position the BG rows separately (usually on
either side of the signal peak); `On` includes a row and Delete removes it.
For nonoverlapping BG intervals, H is the sum of their gated projections and
`alpha = signal width / sum(BG widths)`. For a signal of width 10 and two BG
windows of width 10 each, the correction is `0.5 * (H1 + H2)`. Overlapping BG
bins count once; all widths are actual selected-bin physical widths after the
chosen Manual overlap rule. `Report` lists the actual windows and coefficients.

Only controls relevant to the current method are enabled. Manual f is used only
by Fraction=Manual: **f=0 intentionally gives Scaled BG=0 and Net=Raw**. The
status line displays this explicitly; selecting Template=Manual alone does not
select Fraction=Manual. TSpectrum iterations/options apply to the automatic
curve; fit-range controls apply to the local fitting estimators.

Template+TSpectrum passes finite signed projection bins unchanged to ROOT's
TSpectrum, matching jROOT's input handling. It also retains signed estimated or
imported curve values. No clipping or source-bin/error modification is performed.
This does not relax the nonnegative projection/background requirements of the
separate RadWare model.

`Update` commits all editors through one validation path and recalculates.
`Auto Update` does so after a short editing pause, or after releasing a marker or
slider drag. Invalid text stays in the editor with a diagnostic. Incomplete or
mathematically invalid settings retain the last successful spectra and mark both
plots and the report `OUT OF DATE`. The error appears above the canvases and
at the start of Report. Native X-axis zoom and log modes survive
updates. Display histograms are detached copies, so native edits cannot alter
the stored calculation snapshot. Changes made externally to the source require
an explicit `Update`.

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

Every projection also owns `subtracted` and `net` spectra, statistical-variance
and scale-uncertainty components. With `background.model=None`, `subtracted` is
zero and `net` equals `raw`. MatrixGate has no link-time dependency on PhotoPeakFit
or GRSISort; its optional native fitting connector loads PhotoPeakFit at runtime.
Export remains a later stage.

### Template background subtraction

`SubtractionModel::Template` uses `R = S - alpha H`, with separate settings for
the template H and the background fraction f. Except for Width normalization,
`alpha = f * integral(S) / integral(H)`. Both integrals use the same output-axis
domain, defaulting to normal bins. A custom domain uses
`useNormalizationRange=true`, `normalizationLow`, and `normalizationHigh`;
`Projection::normalizationFirst/Last` records the actual bins. Empty template
selections and nonpositive template integrals fail explicitly.

For example, following the small matrix above:

```cpp
MatrixGate::Config subtract;
subtract.gates = {{1, "Signal", 1, 2, true}};
subtract.background.model = MatrixGate::SubtractionModel::Template;
subtract.background.selection = MatrixGate::BackgroundTemplate::Anti;
subtract.background.fractionMethod = MatrixGate::FractionMethod::Manual;
subtract.background.fraction = 0.25;
auto sub = MatrixGate::Compute(matrix, subtract);
std::cout << sub.Report();
// After checking sub.success:
sub.projections[0].net->Draw("hist");
```

| Template | Source bins |
|---|---|
| `None` | Disable subtraction |
| `Full` | All enabled gate-axis bins, including the signal |
| `Anti` | Full with the selected signal bins removed |
| `Compton` | Starts two bins above the upper signal bin by default; a successful local fit uses centroid + 3 sigma |
| `Manual` | Union of enabled `background.gates`, using absolute coordinates |
| `ManualRelative` | Background coordinates are offsets from the midpoint of the requested signal interval; Union uses its enclosing interval |

Selecting `None` keeps stored inactive template settings and returns the raw
spectrum as the net spectrum.

`comptonGapBins` changes the default Compton gap (1 means one skipped bin).
All templates obey the explicit flow policy; unlike jROOT's Compton path,
MatrixGate does not force overflow inclusion. Estimated-fraction and Width
modes require finite normal gate bins. Compton selections with no available
bins fail instead of moving the template back into the signal gate.

Manual windows have their own unique nonzero IDs. `manualOverlap=Reference`
removes signal bins strictly enclosed by an individual background window and
retains partial overlaps, following the reference single-gate rule. Choose
`ExcludeSignal` to remove every overlap or `Allow` to retain all overlaps.
Background windows form a union after their masks are constructed. Anti and
Manual gaps are built by explicit source-bin masks, preserving weighted errors
in both side segments rather than subtracting histogram errors or constructing
reversed intervals.

| Fraction method | Calculation |
|---|---|
| `Manual` | User-supplied `fraction` in [0,1]; this is f, not alpha |
| `TSpectrum` | Estimated background under the signal divided by gated total-projection counts |
| `LinearFixed` | Gaussian plus an endpoint-derived fixed linear baseline |
| `PolynomialFlex` | Gaussian plus a fitted quadratic baseline |
| `Width` | Direct `alpha = signal physical width / template physical width` |

Template selection determines the background spectrum **shape** H. Fraction
selection determines **how much** of that shape is subtracted. Estimated fractions
come from a baseline under the signal gate on the gate-axis total projection;
they do not measure the geometric overlap of signal and background windows.
If the two windows select exactly the same source bins (and the overlap policy
retains them), H=S and the default normalization gives alpha=f, so
`Net=(1-f)*Raw`. A small TSpectrum baseline fraction therefore retains a large
peak even for identical windows. Manual f=1 cancels identical windows. Width
ratio bypasses fraction estimation and uses the actual selected physical widths;
identical masks have alpha=1 and Net=0. Nearby but different windows need not
cancel, and a flat residual alone does not establish a valid background choice.

TSpectrum settings are `spectrumIterations` (default 25) and `spectrumOptions`,
for example `"BackOrder4 nosmoothing"`. The 25-, 10-, and 30-iteration/order-4
reference settings can be selected explicitly. At least `2 * iterations + 1`
normal bins and finite total-projection counts are required. Template mode preserves
signed bins; RadWare retains its nonnegative-count constraints. Options are
validated; drawing options such as `same` are rejected. TSpectrum works in bin
index space, including for variable-width axes.

An existing background curve can instead be supplied as
`projectionBackground`, a vector of exactly N normal-bin **counts** on the gate
axis. In Template mode select the TSpectrum fraction method to use this imported
curve instead of estimating one. The resulting curve is retained independently
in `Result::estimatedBackground` and treated as fixed.

`fractionIntegral=Counts` is the default and sums bin counts without another
width factor. `WidthWeighted` explicitly reproduces the width-weighted
fraction-integral convention of the jROOT selector. These fractions agree on
uniform grids but can differ on variable bins. The alpha normalization integrals
remain sums of counts in either case. Width mode uses physical bin widths and
does not use a fitted/manual fraction; its reported f is derived and may exceed
one. If its signal integral is nonpositive, f is marked undefined while alpha
and the negative net bins remain valid.

Local fits use a separate ROOT::Fit::Fitter, preserving the global
TVirtualFitter used by other tools. They fit exact Gaussian/polynomial bin
integrals in Counts mode, or bin averages in WidthWeighted mode. Physical
coordinates are shifted to the fit midpoint for numerical stability. By default
the fit range extends two signal widths on each side; set `useFitRange`,
`fitLow`, and `fitHigh` to override it. A single connected interval and enough
nonzero-error bins are required. A disconnected Union must use Manual or
TSpectrum, or be analyzed as Separate/Sum. Failed fits are reported, not replaced
with another estimator. Estimated fractions are clipped to [0,1] as in the
reference selector, with both the original value and clipping flag recorded.
Local-fit initialization/minimization is independent code and does not promise
bit-for-bit agreement with jROOT's automatic fit-range heuristics.

For Sum, each signal gate gets its own template and alpha before the results
are added. For Union, the merged signal selection is normalized once. Per-gate
IDs, weights, alpha, f, selected background windows, and fit status are retained
in `Projection::components`, including the fitted parameters, polynomial origin,
chi-square, and NDF. `backgroundTemplate` is the unscaled sum of the
component templates; `subtracted` is the actual weighted sum that was removed.

### RadWare standard 2D background

Select `background.model=SubtractionModel::RadWareStandard` and explicitly set
`symmetricGammaGamma=true`. The matrix must be symmetric within the configured
relative `symmetryTolerance`, with identical X/Y bin edges. The mode excludes
flow bins and uses the whole normal matrix for normalization. Its background
curve b comes from `projectionBackground` or TSpectrum, with finite nonnegative
counts. Estimated b can exceed observed P in individual bins, including through
TSpectrum smoothing. The curve is retained without clipping; the number of
affected bins and maximum excess are recorded and reported. Negative estimated
peak contributions are retained in the original formula. This mode is intended for symmetric gamma-gamma
data, not a generic energy-time or particle-gamma matrix.

With `P_i=sum_j M_ij` and `T=sum_i P_i`, it calculates
`B_ij=(P_i b_j + b_i P_j - b_i b_j)/T`. For each signal selection G, the
subtracted output is `C_j=(b_G/T)P_j + ((P_G-b_G)/T)b_j`. Both coefficients are
reported as alpha and beta. This includes the second background term absent
from Full template subtraction. E2-bump and valley corrections are not included.

### Uncertainty conventions

`errors=FixedCoefficients` is the default. Template net variances use the final
combined cell coefficient `g_i - sum_k(alpha_k h_ki)`, squared before multiplying
the source variance. This includes signal/template overlap and reuse of the
same background or signal cells across Sum gates. The RadWare model follows
the fixed-background convention, retaining the raw gate's statistical variance.

| Error policy | Statistical convention | Automatic relative scale uncertainty |
|---|---|---|
| `FixedCoefficients` | Shared-cell covariance for templates; fixed background for RadWare | 0 |
| `JRootCompatible` | Omit signal/template covariance; retain combined Sum weights | 8% for Manual fraction, 4% for estimated/Width modes |
| `RadWareCompatible` | Raw statistical variance plus scale term, then a variance floor of 1 | 5% |

The compatibility policies apply only to their respective models. Set
`relativeUncertainty >= 0` to override a preset, including explicit zero; -1
selects the preset. These percentages are empirical reference settings.

The scale uncertainty is one common nuisance for the final subtraction:
`systematicShift_j = relativeUncertainty * C_j`. Its cross-bin covariance is
the outer product of that vector. `Projection::Covariance(i,j)` includes this
term, the diagonal statistical variance, and any separately stored variance
floor. The returned TH1 errors contain only the covariance diagonal. Additional
uncertainty/correlation from data-derived f, fitted baselines, P, b, and T is
not propagated: reported errors are explicitly conditional on those quantities.
Different Separate spectra can still share cells and a scale nuisance; a joint
covariance across different Projection objects is not currently returned.

Reference baselines:
[jROOT v4.1 selector](https://github.com/jsmallcombe/jRootAnalysisTools/blob/v4.1/src/j_gating_select_frame.cpp),
[template subtraction](https://github.com/jsmallcombe/jRootAnalysisTools/blob/v4.1/src/j_gating_subtraction_frame.cpp),
[SLICE](https://github.com/radforddc/rw05/blob/fc0b431e5d8146095c054b4a2babd491d4130448/src/slice.c),
[RadWare background derivation](https://github.com/radforddc/rw05/blob/fc0b431e5d8146095c054b4a2babd491d4130448/doc/bgsub.tex),
[ESCL8R error convention](https://github.com/radforddc/rw05/blob/fc0b431e5d8146095c054b4a2babd491d4130448/src/escl8ra.c).

The calculation lives in `MatrixGate/common/`, the public types in
`MatrixGate/include/MatrixGate/`, and the ROOT entry, dictionary, and loader in
`MatrixGate/root/`. Independent session state and native controls live in
`MatrixGate/session/` and `MatrixGate/gui/`. The optional snapshot connector lives
in `MatrixGate/connectors/photopeakfit/`; PhotoPeakFit exposes its versioned C
bridge in `PhotoPeakFit/PhotoPeakBridge.h`. Export remains a later stage.

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
rejection of invalid background settings. Its loader smoke test calls
the public API through Cling and checks shared-bin statistical propagation.
The background tests check Full/Anti/Compton/Manual/Relative masks, weighted
sideband gaps, conditional covariance and scale presets, variable-bin width and
fraction conventions, per-gate Sum normalization, TSpectrum's direct array API,
known Gaussian/linear/quadratic baselines, the explicitly summed RadWare 2D
formula, variance floors, negative net bins, and source preservation.
The session checks cover axis-specific settings and retention of failed drafts
and historical snapshots. With a native desktop GUI, run:

```bash
tests/matrixgate-build/matrixgate-tests/matrixgate_session_tests --gui
```

This checks widget commits, two-edge picks, double-clicks, ROOT-native marker
drags, per-gate slider deferral, automatic updates, per-canvas native keys,
independent rebin/undo, visible-range Y scaling, Shift+E Apply/Cancel,
preservation of visible gates,
proportional zoom mapping, relative-window limits, model switching, multiple
sources, complete gate deletion, native widget reclamation, file/source deletion,
and repeated window cleanup. `--demo`
also saves native Gates/Background/Report screenshots in `tests/matrixgate-tmp/`.

`tests/matrixgate-build/matrixgate-tests/matrixgate_fit_tests` runs the native
PhotoPeakFit connector checks without linking PhotoPeakFit into the test program:
lazy loading, single/multiple peak fits, exact layer snapshots, stale-source
blocking, explicit refresh with retained seeds and fitter binning, per-window
key isolation, gate/source deletion, axis changes, and independent session cleanup.
