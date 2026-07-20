# root-plugins

Optional plugins for ROOT-based applications. Each plugin keeps its ROOT-only
common implementation separate from application-specific connectors.

Each plugin is a parallel top-level unit with its own public API, common code,
connectors, and tests:

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
│   └── tests/
└── FuturePlugin/
    ├── common/
    ├── connectors/
    ├── include/
    └── tests/
```

## Build all plugins

Install ROOT and build Groot before configuring this repository. Groot-specific
connectors use Groot's public Plugin headers and normal build-tree library.

When Groot is in the adjacent `../groot` directory, root-plugins can be built
with one command:

```bash
make
```

This normal build creates only plugin runtime artifacts. Regression-test
executables are opt-in:

```bash
make test
```

For a custom Groot location, use
`make GROOT_SOURCE_DIR=/path/to/groot GROOT_BUILD_DIR=/path/to/groot-build`.

The equivalent direct CMake commands are:

```bash
cmake -S . -B build \
  -DGROOT_SOURCE_DIR="/path/to/groot" \
  -DGROOT_BUILD_DIR="/path/to/groot/build" \
  -DBUILD_TESTING=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

To build only PhotoPeakFit, use its independent CMake entry:

```bash
cmake -S PhotoPeakFit -B build-photopeak \
  -DGROOT_SOURCE_DIR="/path/to/groot" \
  -DGROOT_BUILD_DIR="/path/to/groot/build"
cmake --build build-photopeak -j4
ctest --test-dir build-photopeak --output-on-failure
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
Use `f` to fit, `n` to clean, `w/q` to rebin/undo, left/right arrows to pan,
`b` to display the PhotoPeak background, and `o` to unzoom. Other Groot input
bindings are suppressed only in the session-owned pad. Closing the fit window
or selecting `Exit mode` restores Groot interaction and retains fitted curves.
Background algorithm options are selected from a checked popup menu; direction,
polynomial order, and smoothing choices are mutually exclusive groups.

The plugin is loaded only after the action is selected. Removing
`GROOT_PLUGIN_PATH` leaves Groot independent of this repository.

The tests use
`../macros_root/158_12_05_25_back_subtracted.root` by default. Override it
when needed with `-DPHOTOPEAK_TEST_DATA=/absolute/path/to/file.root`. They
cover the ROOT-only single/multiple-peak baseline, session interaction, the
Groot connector, and manifest discovery with lazy dynamic loading.
