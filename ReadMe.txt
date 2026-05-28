# Maya NIF Plugin (DCCStudios Fork)

A Maya plug-in for importing and exporting NetImmerse/Gamebryo NIF files
used by Bethesda games (Skyrim LE/SE/AE, Fallout 3/NV/4).

| | |
|---|---|
| **Fork** | [DCCStudios/Maya_NifTools_Plugin](https://github.com/DCCStudios/Maya_NifTools_Plugin) |
| **Upstream** | [Alecu100/maya_nif_plugin](https://github.com/Alecu100/maya_nif_plugin) |

---

## What's Different in This Fork

The upstream repository (by Alecu100) was last updated in **May 2017** and
targets **Maya 2012 – 2017**. It uses only the **niflib** library for NIF
reading/writing, which does not understand Skyrim SE/AE's `BSTriShape`
geometry nodes — importing any SE-era NIF would crash or produce empty meshes.

This fork (by TheShinyHaxorus / DCCStudios) modernizes the plugin for
**Maya 2025** and adds a second NIF backend
([nifly](https://github.com/ousnius/nifly)) for full modern NIF support.

### 1 — Maya 2025 Support

- New **`Release - 2025 | x64`** build configuration targeting the Maya 2025 devkit.
- C++17 language standard enabled (`/std:c++17`).
- `WIN32_LEAN_AND_MEAN` / `NOMINMAX` defines to prevent Windows header conflicts.
- Post-build step auto-deploys the `.mll`, DLL, and MEL scripts to Maya 2025 user dirs.
- Import scale option wired into the options UI.

### 2 — Nifly Importer (Modern NIF Support)

[nifly](https://github.com/ousnius/nifly) — the same library used by Outfit Studio /
BodySlide — is added as a Git submodule under `extern/nifly` and correctly handles all
modern NIF versions.

A new `NiflyImporter` class auto-detects modern NIFs (user-version >= 12, BSFadeNode
root, etc.) and routes them through nifly instead of niflib:

| Feature | Details |
|---------|---------|
| **Mesh import** | Vertices, normals, UVs, vertex colors, triangles |
| **Materials** | Maya Phong shaders with diffuse, normal, and glow maps |
| **Texture paths** | Resolved via user-configured search paths from the import options dialog |
| **Skeleton / skinning** | Full joint hierarchy, `skinCluster` with per-vertex bone weights, optional normalization |
| **Reference skeleton** | Sources bone transforms from an external skeleton NIF when the mesh NIF has names only |
| **Dismember partitions** | Reads `BSDismemberSkinInstance` and creates `nifDismemberPartition` Maya nodes |

### 3 — Nifly Exporter & Resave Bridge

- `NiflyExportingFixture` for native nifly-based Skyrim geometry export.
- `ResaveWithNifly()` bridge can re-save a niflib-exported NIF through nifly for
  format consistency.

### 4 — Updated niflib Submodule

niflib submodule changed from `Alecu100/niflib` to
[DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib), which adds
BSTriShape stub support so the legacy code path no longer crashes on SE NIFs.

### 5 — Fallout 4 Code Removed

The upstream Fallout 4 classes (`NifImportingFixtureFallout4`,
`NifMeshImporterFallout4`, `BSSegment`, `BSSubSegment`, etc.) were incomplete and
non-functional. They have been removed — modern Fallout 4 NIFs are handled by
nifly instead.

### 6 — Code Cleanup

- Skyrim fixtures renamed (`NifSkyrimImportingFixture` / `NifSkyrimExportingFixture`).
- `NifMaterialImporterSkyrimFallout4` simplified to `NifMaterialImporterSkyrim`.
- Removed `.vcxproj.user` with hardcoded user paths.
- Added `.gitignore` for build artifacts.
- PDB output redirected to `$(TEMP)` to avoid file-locking issues.

---

## Building from Source

Everything you need to compile the plugin from a fresh clone. The build
produces two files:

- **`nifTranslator.mll`** — the Maya plug-in
- **`niflib_x64.dll`** — runtime library loaded alongside the plugin

<br>

### Prerequisites

| Requirement | Notes |
|-------------|-------|
| **Windows 10 / 11 x64** | |
| **Visual Studio 2022** | Community, Professional, or Enterprise. Install the **"Desktop development with C++"** workload. Uses the **v143** toolset. |
| **CMake 3.10+** | Only needed to build the nifly static library (Step 3). |
| **Git** | Any recent version with submodule support. |
| **Maya 2025 DevKit** | Separate download from [Autodesk Developer Network](https://www.autodesk.com/developer-network/platform-technologies/maya) (free Autodesk account required). Download the **Windows** devkit zip for Maya 2025 and extract it anywhere. |

> **Note:** You do *not* need Maya itself installed to compile, but you will need
> it to run / test the plugin.

<br>

### Step 1 — Clone the Repository

```
git clone --recursive https://github.com/DCCStudios/Maya_NifTools_Plugin.git
cd Maya_NifTools_Plugin
```

> The **`--recursive`** flag is critical — it pulls two required submodules:
>
> | Submodule | Local path | Source |
> |-----------|------------|--------|
> | niflib | `niflib/` | [DCCStudios/Mayaniflib](https://github.com/DCCStudios/Mayaniflib) |
> | nifly | `extern/nifly/` | [ousnius/nifly](https://github.com/ousnius/nifly) |

If you already cloned without `--recursive`:

```
git submodule update --init --recursive
```

Verify both directories are populated:

```
dir niflib\include
dir extern\nifly\include
```

Both should list header files. If either is empty, the submodule didn't pull —
re-run the submodule command above.

<br>

### Step 2 — Configure the Maya 2025 DevKit Path

The `.vcxproj` ships with a hardcoded devkit path that you almost certainly need
to change. Open **`maya_nif_plugin.vcxproj`** in any text editor and search for
**`Release - 2025`**. You'll find two blocks to update:

**A) Compiler include paths** (around line 1185):

```xml
<AdditionalIncludeDirectories>
  ...
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\include;
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\include\maya;
  ...
</AdditionalIncludeDirectories>
```

**B) Linker library paths** (around line 1198):

```xml
<AdditionalLibraryDirectories>
  ...
  E:\Skyrim Animation\SKSE\Maya Scripts\Autodesk_Maya_2025_DEVKIT_Windows\devkitBase\lib;
  ...
</AdditionalLibraryDirectories>
```

Replace the `E:\Skyrim Animation\...` portion with wherever you extracted your
devkit. For example, if you extracted to `C:\Maya2025Devkit`:

```
C:\Maya2025Devkit\devkitBase\include
C:\Maya2025Devkit\devkitBase\include\maya
C:\Maya2025Devkit\devkitBase\lib
```

Your devkit folder should have this structure:

```
<your-devkit>\
 └─ devkitBase\
     ├─ include\
     │   └─ maya\
     │       ├─ MFnPlugin.h
     │       ├─ MObject.h
     │       └─ ...
     └─ lib\
         ├─ Foundation.lib
         ├─ OpenMaya.lib
         ├─ OpenMayaAnim.lib
         └─ ...
```

<br>

### Step 3 — Build nifly (Static Library)

The plugin statically links **`nifly.lib`**. Nifly uses CMake and must be built
before the main solution.

Open a **Developer Command Prompt for VS 2022** (or any terminal where `cmake`
is available) and run from the repo root:

```
cd extern\nifly
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

This produces:

```
extern\nifly\build\src\Release\nifly.lib
```

The main project's linker expects `nifly.lib` at a specific path. Copy it there:

```
mkdir "..\..\extern\nifly\x64\Release - 2025" 2>nul
copy build\src\Release\nifly.lib "..\..\extern\nifly\x64\Release - 2025\nifly.lib"
```

> **Alternative:** Instead of copying, you can edit the
> `AdditionalLibraryDirectories` for `Release - 2025` in
> `maya_nif_plugin.vcxproj` to point directly at
> `extern\nifly\build\src\Release`.

<br>

### Step 4 — Build niflib (Automatic)

niflib builds as a DLL (`niflib_x64.dll`) and its `.vcxproj` is already
included in the solution with a matching `Release - 2025 | x64` configuration.

**No manual steps needed** — it builds automatically as a dependency when you
build the solution in Step 5.

It outputs:

```
niflib\bin\niflib_x64.dll      (runtime DLL — deployed alongside the plugin)
niflib\lib\niflib_dll.lib      (import library — used at link time only)
```

Just make sure the `niflib/` submodule directory is populated (Step 1).

<br>

### Step 5 — Build the Plugin

1. Open **`maya_nif_plugin.sln`** in Visual Studio 2022.
2. Set the configuration dropdown to **`Release - 2025`** and platform to **`x64`**.
3. **Build → Build Solution** (`Ctrl+Shift+B`).

The solution compiles two projects in dependency order:

| # | Project | Output |
|---|---------|--------|
| 1 | niflib | `niflib\bin\niflib_x64.dll` + `niflib\lib\niflib_dll.lib` |
| 2 | maya_nif_plugin | `Release - 2025\nifTranslator.mll` |

The final plugin (`nifTranslator.mll`) links:

- **`nifly.lib`** — statically baked into the `.mll`
- **`niflib_dll.lib`** — import lib for the niflib DLL
- **Maya SDK** — `Foundation.lib`, `OpenMaya.lib`, `OpenMayaAnim.lib`,
  `OpenMayaUI.lib`, `OpenMayaFX.lib`, `OpenMayaRender.lib`, `Image.lib`

<br>

### Step 6 — Deploy to Maya

The **post-build step runs automatically** after a successful build and copies
everything to the correct Maya directories:

**Plug-ins** → `%USERPROFILE%\Documents\maya\2025\plug-ins\`

| File | Purpose |
|------|---------|
| `nifTranslator.mll` | The plugin itself |
| `niflib_x64.dll` | Runtime library (must be next to the `.mll`) |

**Scripts** → `%USERPROFILE%\Documents\maya\2025\scripts\`

| File | Purpose |
|------|---------|
| `nifTranslatorOpts.mel` | Import/export options dialog |
| `nifTranslatorMenuCreate.mel` | Creates the NifTools menu |
| `nifTranslatorMenuRemove.mel` | Removes the NifTools menu on unload |
| `AEbsLightningShaderTemplate.mel` | Attribute editor for BS Lightning Shader |
| `AEnifDismemberPartitionTemplate.mel` | Attribute editor for dismember partitions |

> If the post-build step fails (e.g. paths differ on your system), copy these
> files manually. The two critical ones are `nifTranslator.mll` and
> `niflib_x64.dll` — they must both be in Maya's `plug-ins` folder.

> **Different Maya version?** Change the folder name from `2025` to match yours
> and make sure you compiled against the corresponding devkit.

<br>

### Step 7 — Load the Plugin in Maya

1. Launch **Maya 2025**.
2. Go to **Windows → Settings/Preferences → Plug-in Manager**.
3. Find **`nifTranslator.mll`** in the list.
4. Check **Loaded** (and optionally **Auto load** for future sessions).
5. A **NifTools** menu should appear in the menu bar.
6. Use **File → Import** to open `.nif` / `.kf` files, or
   **File → Export Selection** to write `.nif` geometry.

<br>

### Troubleshooting

---

**"Cannot find niflib_x64.dll"**

Make sure `niflib_x64.dll` is in the same directory as `nifTranslator.mll`
(the `plug-ins` folder), or in a directory on your system `PATH`.

---

**"nifly.lib not found" (link error)**

You need to build nifly first (Step 3) and place `nifly.lib` where the project
expects it. Check the `AdditionalLibraryDirectories` in the `Release - 2025`
config of `maya_nif_plugin.vcxproj` for the exact expected path.

---

**"Cannot open include file: maya/MFnPlugin.h"**

The Maya 2025 devkit paths in `.vcxproj` don't match your system. Go back to
Step 2 and update the include/library directories.

---

**Plugin loads but no NifTools menu appears**

The MEL scripts are missing from your Maya scripts directory. Copy them manually
from the repo root to `%USERPROFILE%\Documents\maya\2025\scripts\`:
- `nifTranslatorOpts.mel`
- `nifTranslatorMenuCreate.mel`
- `nifTranslatorMenuRemove.mel`

---

**Import produces an empty scene**

Check the debug log at:
`%USERPROFILE%\Documents\maya\2025\scripts\nifTranslator_debug.log`

Common causes: the NIF version is too old (pre-Skyrim LE) and falls through to
the legacy niflib code path, or the file is corrupt.

---

## Project Structure

```
maya_nif_plugin/
│
├─ NifTranslator.cpp / .h             Main Maya translator entry point
├─ maya_nif_plugin.sln                 Visual Studio solution
├─ maya_nif_plugin.vcxproj             Visual Studio project (build configs)
│
├─ niflib/                             Submodule: niflib (DLL — legacy NIF I/O)
├─ extern/
│   └─ nifly/                          Submodule: nifly (static lib — modern NIF I/O)
│
├─ include/
│   ├─ Common/                         Shared types, options, utilities
│   ├─ Custom Nodes/                   BSLightningShader, NifDismemberPartition
│   ├─ Exporters/                      Export fixture classes
│   └─ Importers/                      Import fixture classes (incl. NiflyImporter)
│
├─ src/                                Implementation files (mirrors include/ layout)
│
├─ nifTranslatorOpts.mel               Import/export options dialog
├─ nifTranslatorMenuCreate.mel         NifTools menu creation script
├─ nifTranslatorMenuRemove.mel         NifTools menu removal script
├─ AEbsLightningShaderTemplate.mel     Attribute editor template
├─ AEnifDismemberPartitionTemplate.mel Attribute editor template
│
└─ ReadMe.txt                          This file
```

---

## Original Project

The original Maya NIF File Translator by Alecu100 was documented at:
http://www.niftools.org/wiki/index.php/Maya
