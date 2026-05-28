# Maya NIF Plugin (DCCStudios Fork)

A Maya plug-in for importing and exporting NetImmerse/Gamebryo NIF files
used by Bethesda games (Skyrim LE/SE/AE, Fallout 3/NV/4).

**Fork:** [DCCStudios/Maya_NifTools_Plugin](https://github.com/DCCStudios/Maya_NifTools_Plugin)
**Upstream:** [Alecu100/maya_nif_plugin](https://github.com/Alecu100/maya_nif_plugin)

---

## Fork Differences from Upstream (Alecu100)

The upstream repository (by Alecu100) was last updated in May 2017 and targets
Maya 2012–2017. It uses only the **niflib** library for NIF reading/writing, which
does not understand Skyrim SE/AE's `BSTriShape` geometry nodes — importing any
SE-era NIF would crash or produce empty meshes.

This fork (by TheShinyHaxorus / DCCStudios) adds the following on top of upstream:

### 1. Maya 2025 Support
- A new **`Release - 2025 | x64`** build configuration for Maya 2025.
- Include and library paths target the Maya 2025 devkit.
- C++17 language standard enabled (`/std:c++17`).
- `WIN32_LEAN_AND_MEAN` and `NOMINMAX` preprocessor defines added to prevent
  Windows header conflicts with Maya and nifly headers.
- Post-build step automatically deploys the built `.mll` plugin, `niflib_x64.dll`,
  and all required MEL scripts to the correct Maya 2025 user directories.
- Import scale option wired into the options UI.

### 2. Nifly Importer (BSTriShape / Modern NIF support)
- Added [nifly](https://github.com/ousnius/nifly) as a Git submodule under
  `extern/nifly`. Nifly is the same NIF library used by Outfit Studio / BodySlide
  and correctly handles all modern NIF versions.
- New `NiflyImporter` class detects modern NIF files automatically
  (user-version ≥ 12, BSFadeNode root, etc.) and imports them through nifly
  instead of niflib.
- Full mesh import: vertices, normals, UVs, vertex colors, triangles.
- Material / texture import: creates Maya Phong shaders with diffuse, normal, and
  glow texture maps; resolves texture paths using the user-configured search paths
  from the import options dialog.
- Skeleton / skinning import: builds the full joint hierarchy from the NIF, creates
  Maya joints, applies `skinCluster` with per-vertex bone weights, and supports
  optional weight normalization.
- Reference skeleton loading: can source bone transforms from an external skeleton
  NIF when the mesh NIF only contains bone names without hierarchy data.
- Dismember partition import: reads `BSDismemberSkinInstance` partition info and
  creates matching `nifDismemberPartition` custom nodes with blind-data face
  assignments in Maya.

### 3. Nifly Exporter and Resave Bridge
- New `NiflyExportingFixture` for native nifly-based geometry export of Skyrim
  meshes.
- `ResaveWithNifly()` bridge utility can re-save a niflib-exported NIF through
  nifly for format consistency.
- Skyrim material exports now route through nifly directly.

### 4. Updated niflib Submodule
- niflib submodule changed from `Alecu100/niflib` to
  [DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib), which adds
  BSTriShape stub support so the legacy code path no longer crashes on SE NIFs.

### 5. Fallout 4 Code Removed
- The upstream Fallout 4 importer/exporter classes (`NifImportingFixtureFallout4`,
  `NifExportingFixtureFallout4`, `NifMeshImporterFallout4`, `NifMeshExporterFallout4`,
  `NifNodeImporterFallout4`, `BSSegment`, `BSSubSegment`) were incomplete and
  non-functional. They have been removed; modern Fallout 4 NIFs are now handled by
  the nifly import/export path instead.

### 6. Code Cleanup
- Skyrim fixture files renamed for consistency
  (`NifSkyrimImportingFixture` / `NifSkyrimExportingFixture`).
- `NifMaterialImporterSkyrimFallout4` renamed to `NifMaterialImporterSkyrim`.
- Removed `.vcxproj.user` with hardcoded user paths.
- Added `.gitignore` for build artifacts (`x64/`, `Release*/`, `*.mll`, `*.dll`,
  IDE temp files).
- PDB output redirected to `$(TEMP)` to avoid file-locking issues.

---

## Building from Source

This section walks you through everything needed to compile the plugin from a
fresh clone. The build produces `nifTranslator.mll` — a Maya plug-in DLL — and
the supporting `niflib_x64.dll` runtime library.

### Prerequisites

You will need:

- **Windows 10/11 x64**
- **Visual Studio 2022** (Community, Professional, or Enterprise) with the
  **"Desktop development with C++"** workload installed. The project uses the
  **v143** platform toolset and **C++17**.
- **Git** (any recent version) with submodule support.
- **Autodesk Maya 2025** installed (or at minimum the Maya 2025 Devkit).
- **Maya 2025 Development Kit (Devkit)** — a separate download from Autodesk
  containing the C++ headers and import libraries needed to compile Maya plug-ins.
  Download it from
  [Autodesk Developer Network](https://www.autodesk.com/developer-network/platform-technologies/maya)
  (requires an Autodesk account). Look for the **Windows** devkit matching your
  Maya version. It comes as a zip file — extract it anywhere you like.
- **CMake 3.10+** (only needed to build the nifly static library).

### Step 1 — Clone the Repository

```
git clone --recursive https://github.com/DCCStudios/Maya_NifTools_Plugin.git
cd Maya_NifTools_Plugin
```

The `--recursive` flag is important — it pulls two required submodules:

| Submodule | Path | Repo |
|-----------|------|------|
| niflib | `niflib/` | [DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib) |
| nifly | `extern/nifly/` | [ousnius/nifly](https://github.com/ousnius/nifly) |

If you already cloned without `--recursive`, run:

```
git submodule update --init --recursive
```

Verify both directories are populated:
```
dir niflib\include
dir extern\nifly\include
```

### Step 2 — Point the Project at Your Maya 2025 Devkit

Open `maya_nif_plugin.vcxproj` in a text editor and search for `Release - 2025`.
You will find two path references that need to match your devkit location:

**Compiler include paths** (line ~1185):
```xml
<AdditionalIncludeDirectories>
  ...
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\include;
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\include\maya;
  ...
</AdditionalIncludeDirectories>
```

**Linker library paths** (line ~1198):
```xml
<AdditionalLibraryDirectories>
  ...
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\lib;
  ...
</AdditionalLibraryDirectories>
```

Replace both occurrences of the devkit base path with wherever you extracted your
devkit. For example, if you extracted to `C:\Maya2025Devkit`, you would change the
paths to:
```
C:\Maya2025Devkit\devkitBase\include
C:\Maya2025Devkit\devkitBase\include\maya
C:\Maya2025Devkit\devkitBase\lib
```

The devkit folder structure should look like:
```
<your-devkit-path>\
  devkitBase\
    include\
      maya\
        MFnPlugin.h
        MObject.h
        ...
    lib\
      Foundation.lib
      OpenMaya.lib
      OpenMayaAnim.lib
      ...
```

### Step 3 — Build nifly (Static Library)

The plugin links against `nifly.lib` (a static library). Nifly uses CMake and
must be built separately before the main solution.

Open a **Developer Command Prompt for VS 2022** (or any terminal with CMake
available) and run:

```
cd extern\nifly
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

This produces `nifly.lib` inside `build\src\Release\`.

The main project expects `nifly.lib` at:
```
extern\nifly\x64\Release - 2025\nifly.lib
```

Create that directory and copy the lib there:
```
mkdir "..\..\extern\nifly\x64\Release - 2025" 2>nul
copy build\src\Release\nifly.lib "..\..\extern\nifly\x64\Release - 2025\nifly.lib"
```

Alternatively, edit the `AdditionalLibraryDirectories` for the `Release - 2025`
config in `maya_nif_plugin.vcxproj` to point at wherever your `nifly.lib` ended up
(e.g. `extern\nifly\build\src\Release`).

### Step 4 — Build niflib (DLL)

niflib builds as a **dynamic library** (`niflib_x64.dll` + `niflib_dll.lib`
import lib). Its project file (`niflib\niflib.vcxproj`) is included in the
solution and has its own `Release - 2025 | x64` configuration, so it **builds
automatically** when you build the solution in the next step.

niflib outputs:
```
niflib\bin\niflib_x64.dll     ← runtime DLL (deployed alongside the plugin)
niflib\lib\niflib_dll.lib     ← import library (used at link time)
```

No manual steps needed — just make sure the `niflib/` submodule directory is
populated (Step 1).

### Step 5 — Build the Plugin

1. Open **`maya_nif_plugin.sln`** in Visual Studio 2022.
2. In the toolbar, set the configuration to **`Release - 2025`** and the platform
   to **`x64`**.
3. Build the solution: **Build → Build Solution** (or press `Ctrl+Shift+B`).

The build compiles two projects in order:

| Project | Output |
|---------|--------|
| niflib | `niflib\bin\niflib_x64.dll` + `niflib\lib\niflib_dll.lib` |
| maya_nif_plugin | `Release - 2025\nifTranslator.mll` |

The main plugin links against:
- `nifly.lib` (static — baked into the .mll)
- `niflib_dll.lib` (import lib for the niflib DLL)
- Maya SDK libs (`Foundation.lib`, `OpenMaya.lib`, `OpenMayaAnim.lib`,
  `OpenMayaUI.lib`, `OpenMayaFX.lib`, `OpenMayaRender.lib`, `Image.lib`)

### Step 6 — Deploy to Maya

The post-build step runs automatically on a successful build and copies
everything to the right places:

| File | Destination |
|------|-------------|
| `nifTranslator.mll` | `%USERPROFILE%\Documents\maya\2025\plug-ins\` |
| `niflib_x64.dll` | `%USERPROFILE%\Documents\maya\2025\plug-ins\` |
| `nifTranslatorOpts.mel` | `%USERPROFILE%\Documents\maya\2025\scripts\` |
| `nifTranslatorMenuCreate.mel` | `%USERPROFILE%\Documents\maya\2025\scripts\` |
| `nifTranslatorMenuRemove.mel` | `%USERPROFILE%\Documents\maya\2025\scripts\` |
| `AEbsLightningShaderTemplate.mel` | `%USERPROFILE%\Documents\maya\2025\scripts\` |
| `AEnifDismemberPartitionTemplate.mel` | `%USERPROFILE%\Documents\maya\2025\scripts\` |

It also attempts to copy `niflib_x64.dll` to `C:\Program Files\Autodesk\Maya2025\bin\`
(this requires admin privileges and is optional — the plug-ins folder copy is
sufficient).

If the post-build step fails (e.g. Maya is not installed at the default path),
you can copy these files manually. The two critical files are:
- `nifTranslator.mll` → Maya's `plug-ins` folder
- `niflib_x64.dll` → same `plug-ins` folder (must be next to the .mll, or on PATH)

The MEL scripts must be in Maya's `scripts` folder for the options dialog and
NifTools menu to work.

**If you are deploying to a different Maya version** (e.g. Maya 2024), change the
folder name from `2025` to match your version and ensure you compiled against the
matching devkit.

### Step 7 — Load the Plugin in Maya

1. Launch **Maya 2025**.
2. Go to **Windows → Settings/Preferences → Plug-in Manager**.
3. Scroll down or search for **`nifTranslator.mll`**.
4. Check **Loaded** (and optionally **Auto load** to load it every time Maya starts).
5. You should see a **NifTools** menu appear in Maya's menu bar.
6. Use **File → Import** to open `.nif` files, or **File → Export Selection** to
   export geometry back to `.nif`.

### Troubleshooting

**"Cannot find niflib_x64.dll"** — Make sure `niflib_x64.dll` is in the same
directory as `nifTranslator.mll` (the `plug-ins` folder), or in a directory on
your system `PATH`.

**"nifly.lib not found" during linking** — You need to build nifly first (Step 3)
and place `nifly.lib` where the project expects it. Check the
`AdditionalLibraryDirectories` in the `Release - 2025` config for the exact path.

**"Cannot open include file: maya/MFnPlugin.h"** — The Maya 2025 devkit paths in
the `.vcxproj` don't match your system. Follow Step 2 to update them.

**Plugin loads but NifTools menu doesn't appear** — The MEL scripts
(`nifTranslatorMenuCreate.mel`, etc.) are missing from your Maya scripts directory.
Copy them manually from the repo root to
`%USERPROFILE%\Documents\maya\2025\scripts\`.

**Import produces empty scene** — Check `%USERPROFILE%\Documents\maya\2025\scripts\nifTranslator_debug.log`
for error messages. Common causes: the NIF version is too old (pre-Skyrim) and the
legacy niflib path has a bug, or the file is corrupt.

---

## Project Structure

```
maya_nif_plugin/
├── NifTranslator.cpp/.h          Main Maya translator entry point
├── maya_nif_plugin.sln            Visual Studio solution
├── maya_nif_plugin.vcxproj        Visual Studio project
├── niflib/                        niflib submodule (DLL, legacy NIF read/write)
├── extern/
│   └── nifly/                     nifly submodule (static lib, modern NIF read/write)
├── include/
│   ├── Common/                    Shared types, options, utilities
│   ├── Custom Nodes/              Maya custom nodes (BSLightningShader, NifDismemberPartition)
│   ├── Exporters/                 Export fixture classes
│   └── Importers/                 Import fixture classes (including NiflyImporter)
├── src/                           Source files mirroring include/ layout
├── *.mel                          MEL scripts for Maya UI (options dialog, menus, AE templates)
└── ReadMe.txt                     This file
```

---

## Original Project

The original Maya NIF File Translator by Alecu100 was documented at:
http://www.niftools.org/wiki/index.php/Maya
