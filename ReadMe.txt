# Maya NIF Plugin (DCCStudios Fork)

A Maya plug-in for importing and exporting NIF files used by Bethesda games (Skyrim, Skyrim SE, Fallout 3/NV/4).

**Fork:** [DCCStudios/Maya_NifTools_Plugin](https://github.com/DCCStudios/Maya_NifTools_Plugin)
**Upstream:** [Alecu100/maya_nif_plugin](https://github.com/Alecu100/maya_nif_plugin)

---

## Fork Differences from Upstream (Alecu100)

The upstream repository was last updated in May 2017 and targets Maya 2012–2017 using only the
niflib library. This fork modernizes the plugin for Maya 2025 and adds a second NIF backend
([nifly](https://github.com/ousnius/nifly)) so that Skyrim SE / AE and Fallout 4 meshes
(BSTriShape-based NIFs) are handled correctly.

### Maya 2025 Support (commit `ecf5554`)
- Added a full `Release - 2025` build configuration targeting the Maya 2025 devkit (x64).
- Updated include/library paths for the Maya 2025 devkit layout.
- Added C++17 language standard (`/std:c++17`).
- Added `WIN32_LEAN_AND_MEAN` and `NOMINMAX` defines to avoid Windows header conflicts.
- Post-build step copies the `.mll`, MEL scripts, and `niflib_x64.dll` to Maya 2025 user dirs.
- Import scale option plumbed through to the importer UI.

### BSTriShape Import via niflib Fallback (commit `8ce7697`)
- Updated the niflib submodule to [DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib)
  which adds BSTriShape stub support so the legacy niflib path no longer crashes on SE NIFs.
- Reader now detects BSFadeNode root and `userVersion 11/12` variants to route Skyrim NIFs
  correctly.

### Nifly Fallback Importer (commit `2dc5968`)
- Introduced `NiflyImporter` class (`include/Importers/NiflyImporter.h`,
  `src/Importers/NiflyImporter.cpp`) — a standalone importer that uses the
  [nifly](https://github.com/ousnius/nifly) library (added as `extern/nifly` submodule).
- `NiflyImporter::ShouldUseNifly()` auto-detects modern NIF versions
  (user-version ≥ 12, SSE/FO4/Starfield) and routes them through nifly instead of niflib.
- Imports meshes with full vertex data (positions, normals, UVs, vertex colors), triangles,
  and Phong-based material/texture assignment.
- Texture path resolution respects the user-configured texture search paths from the options UI
  and probes common data directory layouts (`textures/`, import file directory, etc.).
- Debug logging to `nifTranslator_debug.log` in the Maya scripts folder.

### Nifly-based Exporter and Resave Bridge (commit `0eeb742`)
- Added `NiflyExportingFixture` (`include/Exporters/NiflyExportingFixture.h`,
  `src/Exporters/NiflyExportingFixture.cpp`) for native nifly geometry export of Skyrim meshes.
- Added `NiflyExportBridge` utility (`ResaveWithNifly()`) that can re-save a niflib-exported NIF
  through nifly for consistency.
- Skyrim material exports now route through nifly directly; legacy niflib fallback with nifly
  resave is also supported.

### Fallout 4 Code Removed
- Removed dedicated Fallout 4 importer/exporter fixture classes that were unfinished and
  non-functional (`NifImportingFixtureFallout4`, `NifExportingFixtureFallout4`,
  `NifMeshImporterFallout4`, `NifMeshExporterFallout4`, `NifNodeImporterFallout4`,
  `BSSegment`, `BSSubSegment`).
- Modern Fallout 4 NIF files are now handled by the nifly importer path instead.

### Skyrim Fixture Renames
- `NifImportingFixtureSkyrim` / `NifExportingFixtureSkyrim` header and source files renamed to
  `NifSkyrimImportingFixture` / `NifSkyrimExportingFixture` for consistency.
- `NifMaterialImporterSkyrimFallout4` renamed to `NifMaterialImporterSkyrim` (Fallout 4-specific
  material code removed).

### Solution / Project Cleanup
- Removed `.vcxproj.user` file with hardcoded user paths.
- Cleaned up solution file to remove legacy build configurations that don't apply to 2025.
- Added `.gitignore` entries for build artifacts, `x64/`, and IDE temp files.
- PDB output paths changed to `$(TEMP)` to avoid locking issues with parallel builds.

### Uncommitted Changes (Working Tree)
The current working tree contains additional improvements beyond the last commit:

- **Skinning / bone import** — `NiflyImporter` now builds a full joint hierarchy from the NIF
  skeleton, creates Maya joints via `GetOrCreateJoint()`, and applies skin weights through
  `skinCluster` with proper bone-weight normalization support (controlled by a new
  `importNormalizedWeights` option).
- **Reference skeleton loading** — `LoadReferenceSkeleton()` / `ShouldUseReferenceSkeleton()`
  allow the importer to source bone transforms from an external skeleton NIF when the mesh NIF
  contains only bone names without full hierarchy data.
- **Dismember partition import** — `ApplyDismemberPartitions()` reads
  `BSDismemberSkinInstance` partition info from the NIF and creates matching
  `nifDismemberPartition` nodes in Maya with blind data face assignments.
- **Texture path resolution improvements** — user-configured texture paths are now tried first
  (before the Maya project relative path), and absolute paths that already exist are returned
  as-is.
- **Nifly solution project removed** — the separate nifly `.vcxproj` project reference was
  removed from the `.sln`; nifly is now linked as a pre-built static lib.

---

## Building from Source

### Prerequisites

| Tool | Version |
|------|---------|
| Visual Studio | 2022 (v143 toolset) or later |
| Maya Devkit | Autodesk Maya 2025 Devkit for Windows (x64) |
| C++ Standard | C++17 |
| Git | With submodule support |

### 1. Clone the Repository

```
git clone --recursive https://github.com/DCCStudios/Maya_NifTools_Plugin.git
cd Maya_NifTools_Plugin
```

If you already cloned without `--recursive`:
```
git submodule update --init --recursive
```

This pulls both submodules:
- `niflib` → [DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib)
- `extern/nifly` → [ousnius/nifly](https://github.com/ousnius/nifly)

### 2. Set Up the Maya 2025 Devkit

Download the [Maya 2025 Devkit](https://www.autodesk.com/developer-network/platform-technologies/maya)
and extract it somewhere accessible. The `.vcxproj` expects it at:

```
E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase
```

If your devkit is in a different location, update the include and library paths in
`maya_nif_plugin.vcxproj` under the `Release - 2025|x64` configuration:
- `AdditionalIncludeDirectories` — point to `<devkit>\devkitBase\include` and
  `<devkit>\devkitBase\include\maya`
- `AdditionalLibraryDirectories` — point to `<devkit>\devkitBase\lib`

### 3. Build nifly (if not pre-built)

The plugin links against `nifly.lib`. If you don't have a pre-built copy:

1. Open `extern/nifly` in Visual Studio or build via CMake.
2. Build in **Release x64** mode.
3. Ensure `nifly.lib` ends up in `extern\nifly\x64\Release - 2025\` (or adjust the library
   path in the main `.vcxproj`).

### 4. Build niflib

niflib is included as a submodule and has its own `.vcxproj` in `niflib/`. It is already
referenced as a project dependency in the solution, so it builds automatically.

### 5. Build the Plugin

1. Open `maya_nif_plugin.sln` in Visual Studio 2022.
2. Select configuration **Release - 2025 | x64**.
3. Build the solution (Ctrl+Shift+B).

The output `nifTranslator.mll` is placed in the `Release - 2025/` folder under the project
directory. The post-build step automatically copies it and all required MEL scripts to your
Maya 2025 user plug-in / scripts directories:

```
%USERPROFILE%\Documents\maya\2025\plug-ins\nifTranslator.mll
%USERPROFILE%\Documents\maya\2025\plug-ins\niflib_x64.dll
%USERPROFILE%\Documents\maya\2025\scripts\nifTranslatorOpts.mel
%USERPROFILE%\Documents\maya\2025\scripts\nifTranslatorMenuCreate.mel
%USERPROFILE%\Documents\maya\2025\scripts\nifTranslatorMenuRemove.mel
%USERPROFILE%\Documents\maya\2025\scripts\AEbsLightningShaderTemplate.mel
%USERPROFILE%\Documents\maya\2025\scripts\AEnifDismemberPartitionTemplate.mel
```

### 6. Load in Maya

1. Open Maya 2025.
2. Go to **Windows → Settings/Preferences → Plug-in Manager**.
3. Find `nifTranslator.mll` and check **Loaded** and **Auto load**.
4. Use **File → Import** or **File → Export** to work with `.nif` / `.kf` files.

---

## Original Project

The original Maya NIF File Translator documentation was hosted at:
http://www.niftools.org/wiki/index.php/Maya
