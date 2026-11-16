# root-plugins

PhotoPeakFit fits peaks in 1D ROOT histograms. MatrixGate gates and projects 2D ROOT histograms.

## Build and install

You need CERN ROOT with native GUI support, CMake, and a C++17 compiler. With `root-config` on your `PATH`, run these commands from the repository root.

```bash
make
cmake --install build --prefix "$HOME/.local"
```

Run the plugins in a desktop ROOT session. To skip installation, use the loader macros in `build/`.

## Start PhotoPeakFit in ROOT

```bash
root -l "$HOME/.local/LoadPhotoPeakFit.C"
```

At the ROOT prompt, open your file, draw a 1D histogram, and start PhotoPeakFit. Replace the example file and object names with your own.

```cpp
auto* file = TFile::Open("data.root");
auto* hist = file->Get<TH1>("spectrum");
hist->Draw();
PhotoPeakFit::Start(hist, gPad);
```

The fit report opens with **PhotoPeakFit result for "histogram title" (histogram name)**. The heading appears in bold green in **Results** and in the terminal.

## Start MatrixGate in ROOT

```bash
root -l "$HOME/.local/LoadMatrixGate.C"
```

At the ROOT prompt, open your file and pass a `TH2` to MatrixGate. Keep the file open while using the matrix.

```cpp
auto* file = TFile::Open("data.root");
auto* matrix = file->Get<TH2>("matrix");
MatrixGate::Start(matrix);
```

The dashed signal and background gate boundaries span the total projection on both linear and logarithmic Y scales.

MatrixGate opens PhotoPeakFit on a separate spectrum snapshot. The snapshot uses the same shortcuts and fit report as a standalone PhotoPeakFit session.

## Shortcuts

Click the target plot to give it keyboard focus. PhotoPeakFit canvas shortcuts act on the active pad. MatrixGate view shortcuts work on both the total projection and the result plot.

| Input | PhotoPeakFit | MatrixGate |
| --- | --- | --- |
| Left click | Add a peak or select a nearby peak in **Pick peaks** mode | Click two edges to set a gate on the total projection in **Pick range** mode |
| Shift + left click | Click twice to set the fit-range bounds in **Pick peaks** mode | |
| `f` | Run the fit | Open PhotoPeakFit for the focused plot |
| `Enter` | Run the fit | Apply gate edits on the total projection in **Pick range** mode |
| `Delete` | Delete the selected peak | Delete the selected gate on the total projection in **Pick range** mode |
| `Esc` | Switch to **Browse** mode | Switch to **Browse** mode from the total projection |
| `n` | Clear the fit, peak selections, and fit range | |
| `b` | Recalculate the TSpectrum background | |
| `w` / `q` | Rebin / undo rebinning | Rebin / undo rebinning |
| `l` | Toggle linear/logarithmic Y on the active pad in either interaction mode | Toggle linear/logarithmic Y on the focused projection or result plot in either interaction mode |
| `←` / `→` | Pan the view | Pan the focused plot |
| `o` | Unzoom the axes | Unzoom the focused plot |
| `Shift+E` | Open ROOT’s X-axis range dialog and apply the range across the canvas | Open ROOT’s X-axis range dialog for the focused plot |
| `Ctrl+C` / `Cmd+C` | Copy selected text in **Results** | |
| `Ctrl+A` / `Cmd+A` | Select all text in **Results** | |
