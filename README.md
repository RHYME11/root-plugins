# root-plugins

Optional plugins for ROOT-based applications. Each plugin keeps its ROOT-only
common implementation separate from application-specific connectors.

## Build all plugins

Install ROOT and the Groot Plugin API, then configure with their CMake package
locations available in `CMAKE_PREFIX_PATH`:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/groot-plugin-sdk"
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

To build only PhotoPeakFit, use its independent CMake entry:

```bash
cmake -S PhotoPeakFit -B build-photopeak \
  -DCMAKE_PREFIX_PATH="/path/to/groot-plugin-sdk"
cmake --build build-photopeak -j4
ctest --test-dir build-photopeak --output-on-failure
```

Both forms require a ROOT development installation and an installed
`GrootPlugin` CMake package. The plugin does not include or compile Groot
source files. An optional install places the library and manifest together:

```bash
cmake --install build --prefix "$HOME/.local"
```

## PhotoPeakFit

The first plugin is a minimal single-photopeak fitting closure test. It builds
one shared library and one adjacent ROOT TEnv manifest in
`build/PhotoPeakFit`.

```bash
export GROOT_PLUGIN_PATH="$PWD/build/PhotoPeakFit"
groot -g /path/to/158_12_05_25_back_subtracted.root
```

Draw `ParticleGates/Er/GammaEfficiency_Er`, then select `Peak fit`. The current
closure test uses the fixed range `[175, 210]` and initial centroid `191.75`.

The plugin is loaded only after the action is selected. Removing
`GROOT_PLUGIN_PATH` leaves Groot independent of this repository.

The tests use
`../macros_root/158_12_05_25_back_subtracted.root` by default. Override it
when needed with `-DPHOTOPEAK_TEST_DATA=/absolute/path/to/file.root`. They
cover the ROOT-only numerical baseline, the Groot connector, and manifest
discovery with lazy dynamic loading.
