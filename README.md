# root-plugins

Optional plugins for ROOT-based applications. Each plugin keeps its ROOT-only
common implementation separate from application-specific connectors.

Each plugin is a parallel top-level unit with its own public API, common code,
and connectors:

```text
root-plugins/
├── PhotoPeakFit/
│   ├── common/
│   ├── session/
│   ├── gui/
│   ├── connectors/
│   │   └── groot/
│   ├── include/
│   │   └── PhotoPeakFit/
└── FuturePlugin/
    ├── common/
    ├── connectors/
    └── include/
```

## Build all plugins

Install ROOT and build Groot before configuring this repository. Groot-specific
connectors use Groot's public Plugin headers and normal build-tree library.

When Groot is in the adjacent `../groot` directory, root-plugins can be built
with one command:

```bash
make
```

For a custom Groot location, use
`make GROOT_SOURCE_DIR=/path/to/groot GROOT_BUILD_DIR=/path/to/groot-build`.

The equivalent direct CMake commands are:

```bash
cmake -S . -B build \
  -DGROOT_SOURCE_DIR="/path/to/groot" \
  -DGROOT_BUILD_DIR="/path/to/groot/build"
cmake --build build -j4
```

To build only PhotoPeakFit, use its independent CMake entry:

```bash
cmake -S PhotoPeakFit -B build-photopeak \
  -DGROOT_SOURCE_DIR="/path/to/groot" \
  -DGROOT_BUILD_DIR="/path/to/groot/build"
cmake --build build-photopeak -j4
```

Both forms require a ROOT development installation and a compiled Groot tree.
The plugin includes Groot's public API headers but does not compile Groot
source files. An optional install places the library and manifest together:

```bash
cmake --install build --prefix "$HOME/.local"
```

## PhotoPeakFit

PhotoPeakFit provides ROOT-only single/multiple-photopeak fitting, a pad-owned
session and control GUI, and a thin Groot connector. It builds one shared
library and one adjacent ROOT TEnv manifest in
`build/plugins/PhotoPeakFit`.

```bash
export GROOT_PLUGIN_PATH="$PWD/build/plugins/PhotoPeakFit"
groot -g /path/to/158_12_05_25_back_subtracted.root
```

Draw `ParticleGates/Er/GammaEfficiency_Er`, then select `Peak fit`. This opens a
non-modal control window and gives the selected pad one PhotoPeak
application-overlay session. ROOT-native canvas, axis, object-selection, and
context-menu behavior remains active; only Groot-specific interaction is
suspended. The session locks one explicitly selected TH1, or the sole TH1 in
the pad; ambiguous multi-histogram pads require an explicit selection. A pad
and non-null target can each belong to only one interactive plugin session.

In fit mode, ordinary left clicks/drags set the two red fit-range lines and
Shift-left-click toggles red centroid markers at continuous x coordinates.
Histogram centroid markers and the control window's initial-peak sections stay
synchronized when either side adds or removes a peak. Each visible initial-peak
section has a `Delete` button that removes the corresponding peak seed and red
centroid marker, matching Shift-click removal on the histogram. Remaining peaks
move forward to fill the current page; if the last page becomes invalid, the
window selects the new last page.
Use `f` to fit, `n` to clean, `w/q` to rebin/undo, left/right arrows to pan,
`b` to display the PhotoPeak background, and `o` to unzoom. Other Groot input
bindings are suppressed only in the session-owned pad. Closing the fit window
or selecting `Exit mode` restores Groot interaction and fully discards the
PhotoPeak session. It removes canvas artifacts, releases the histogram clone
and retained ROOT fitter, and schedules the Fit window and all child controls
for safe event-loop deletion. Re-entering Fit mode creates a new empty session
and never resumes old peaks, markers, or parameters.

In fit mode, both `n` and the `Clean` button invoke the session's own cleanup,
independently of Groot's plugin cleanup. PhotoPeak removes its markers, curves,
legend, centroid labels, and displayed background. Cleanup also clears the
saved peak seeds and resets the fit range, so later clicks cannot restore old
markers; range endpoints and peak markers must be selected again. Initial-peak
rows use a three-row paged pool: the complete peak vector remains available to
the fitter and marker controller, while Previous/Next remap only three GUI
sections at a time. A fixed-height viewport reserves the Initial Peak area when
the session is empty; three sections fit without vertical scrolling. Clean
hides the three pooled rows without dynamically destroying Cocoa controls. The
pool is destroyed with the complete Fit window on Exit. Existing Groot markers
on the target histogram are deleted whenever a new PhotoPeak session is
entered.
Background algorithm options are selected from a checked popup menu; direction,
polynomial order, and smoothing choices are mutually exclusive groups.

The plugin is loaded only after the action is selected. Removing
`GROOT_PLUGIN_PATH` leaves Groot independent of this repository.
