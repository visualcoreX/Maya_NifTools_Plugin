# Maya NIF Plugin — DCCStudios Fork

> A Maya plug-in for importing and exporting NetImmerse / Gamebryo **NIF** files
> used by Bethesda games — **Skyrim LE / SE / AE**, **Fallout 3 / NV / 4**.

| | |
|:--|:--|
| **This Fork** | [`DCCStudios/Maya_NifTools_Plugin`](https://github.com/DCCStudios/Maya_NifTools_Plugin) |
| **Upstream** | [`Alecu100/maya_nif_plugin`](https://github.com/Alecu100/maya_nif_plugin) |
| **Target** | Maya **2025** — Windows x64 |

---

## Table of Contents

1. [What's Different in This Fork](#whats-different-in-this-fork)
2. [Building from Source](#building-from-source)
   - [Prerequisites](#prerequisites)
   - [Step 1 — Clone](#step-1--clone-the-repository)
   - [Step 2 — Build nifly](#step-2--build-nifly-static-library)
   - [Step 3 — Build the Plugin](#step-3--build-the-plugin)
   - [Step 4 — Deploy](#step-4--deploy-to-maya)
   - [Step 5 — Load in Maya](#step-5--load-the-plugin-in-maya)
3. [Troubleshooting](#troubleshooting)
4. [Project Structure](#project-structure)

---

## What's Different in This Fork

The **upstream** repo (by Alecu100) was last updated in **May 2017** and targets
**Maya 2012 – 2017**. It relies solely on **niflib** for NIF I/O — a library that
does **not** understand Skyrim SE/AE's `BSTriShape` geometry. Importing any
SE-era NIF with the upstream code would crash or produce empty meshes.

This fork (by **TheShinyHaxorus** / **DCCStudios**) modernizes the plugin for
**Maya 2025** and adds a second NIF backend —
[**nifly**](https://github.com/ousnius/nifly) — for full modern NIF support.

---

### 1. Maya 2025 Support

| Change | Detail |
|--------|--------|
| Build config | New **`Release - 2025 \| x64`** configuration targeting the Maya 2025 devkit |
| C++ standard | **C++17** enabled (`/std:c++17`) |
| Defines | `WIN32_LEAN_AND_MEAN`, `NOMINMAX` to prevent Windows header conflicts |
| Deployment | Post-build step auto-copies `.mll`, DLL, and MEL scripts to Maya 2025 user dirs |
| Options UI | Import scale option wired through to the importer |

---

### 2. Nifly Importer — Modern NIF Support

[**nifly**](https://github.com/ousnius/nifly) (the same library behind
**Outfit Studio / BodySlide**) is added as a submodule at `extern/nifly`.

A new **`NiflyImporter`** class **auto-detects modern NIFs**
(user-version ≥ 12, `BSFadeNode` root, etc.) and routes them through nifly
instead of niflib.

#### Imported Features

| Feature | What it does |
|:--------|:-------------|
| **Mesh geometry** | Vertices, normals, UVs, vertex colors, triangles |
| **Materials & textures** | Creates Maya Phong shaders — diffuse, normal, glow maps |
| **Texture path resolution** | Uses the search paths configured in the import options dialog |
| **Skeleton & skinning** | Builds joint hierarchy, applies `skinCluster` with per-vertex bone weights |
| **Weight normalization** | Optional — controlled by the `importNormalizedWeights` setting |
| **Reference skeleton** | Loads bone transforms from an external skeleton NIF when the mesh NIF has names only |
| **Dismember partitions** | Reads `BSDismemberSkinInstance` → creates `nifDismemberPartition` Maya nodes |

---

### 3. Nifly Exporter & Resave Bridge

- **`NiflyExportingFixture`** — native nifly-based geometry export for Skyrim meshes.
- **`ResaveWithNifly()`** — bridge utility that re-saves a niflib-exported NIF
  through nifly for format consistency.

---

### 4. Updated niflib Submodule

The niflib submodule was changed from `Alecu100/niflib` →
[**`DCCStudios/Mayaniflib`**](https://github.com/DCCStudios/Mayaniflib), which
adds `BSTriShape` stub support so the legacy code path no longer crashes on
SE NIFs.

---

### 5. Fallout 4 Code Removed

The upstream's Fallout 4 classes (`NifImportingFixtureFallout4`,
`NifMeshImporterFallout4`, `BSSegment`, `BSSubSegment`, etc.) were incomplete
and non-functional. They have been **removed** — modern Fallout 4 NIFs are now
handled by the nifly path.

---

### 6. Code Cleanup

- Skyrim fixtures renamed to `NifSkyrimImportingFixture` / `NifSkyrimExportingFixture`.
- `NifMaterialImporterSkyrimFallout4` → `NifMaterialImporterSkyrim`.
- Removed `.vcxproj.user` with hardcoded user paths.
- Added `.gitignore` for build artifacts.
- PDB output redirected to `$(TEMP)` to prevent file-locking.

---

---

## Building from Source

Everything you need to compile the plugin **from a fresh clone** — no external
SDK downloads required. The Maya 2025 headers and import libraries are bundled
in the repo under `extern/maya2025/`.

The build produces two runtime files:

| Output | Role |
|--------|------|
| **`nifTranslator.mll`** | The Maya plug-in |
| **`niflib_x64.dll`** | Runtime library loaded alongside the plugin |

---

### Prerequisites

| Requirement | Details |
|:------------|:--------|
| **OS** | Windows 10 or 11 — **x64** |
| **Visual Studio 2022** | Community / Professional / Enterprise. Install the **"Desktop development with C++"** workload. Uses the **v143** platform toolset. |
| **CMake 3.10 +** | Only needed to build the nifly static library ([Step 2](#step-2--build-nifly-static-library)). |
| **Git** | Any recent version — submodule support required. |

> [!NOTE]
> **No Maya DevKit download needed.** The required Maya 2025 SDK headers and
> import libraries (`Foundation.lib`, `OpenMaya.lib`, etc.) are already included
> in the repo at `extern/maya2025/`. You only need Maya itself installed to
> *run* the plugin — not to compile it.

---

### Step 1 — Clone the Repository

```bash
git clone --recursive https://github.com/DCCStudios/Maya_NifTools_Plugin.git
cd Maya_NifTools_Plugin
```

> [!IMPORTANT]
> **`--recursive` is required.** It pulls two submodules the build depends on:
>
> | Submodule | Local path | Source |
> |:----------|:-----------|:-------|
> | **niflib** | `niflib/` | [`DCCStudios/Mayaniflib`](https://github.com/DCCStudios/Mayaniflib) |
> | **nifly** | `extern/nifly/` | [`ousnius/nifly`](https://github.com/ousnius/nifly) |

**Already cloned without `--recursive`?** Run:

```bash
git submodule update --init --recursive
```

**Verify** the submodules and bundled SDK are populated:

```bash
dir niflib\include
dir extern\nifly\include
dir extern\maya2025\include\maya
```

All three should list header files. If any are empty, re-run the submodule
command above.

---

### Step 2 — Build nifly (Static Library)

The plugin **statically links** `nifly.lib`. Nifly uses **CMake** and must be
built **before** the main solution.

**Open a Developer Command Prompt for VS 2022** (or any terminal with `cmake`
on `PATH`) and run from the repo root:

```bash
cd extern\nifly
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

This produces:

```
extern\nifly\build\src\Release\nifly.lib
```

The main project expects `nifly.lib` at a specific path — **copy it there**:

```bash
mkdir "..\..\extern\nifly\x64\Release - 2025" 2>nul
copy build\src\Release\nifly.lib "..\..\extern\nifly\x64\Release - 2025\nifly.lib"
```

> [!TIP]
> Instead of copying, you can edit `AdditionalLibraryDirectories` for
> `Release - 2025` in `maya_nif_plugin.vcxproj` to point directly at
> `extern\nifly\build\src\Release`.

---

### Step 3 — Build the Plugin

1. Open **`maya_nif_plugin.sln`** in **Visual Studio 2022**.
2. Set the **Configuration** dropdown to **`Release - 2025`** and **Platform** to **`x64`**.
3. **Build → Build Solution** (or `Ctrl + Shift + B`).

The solution compiles two projects in dependency order:

| Order | Project | Output |
|:-----:|:--------|:-------|
| 1 | **niflib** | `niflib\bin\niflib_x64.dll` + `niflib\lib\niflib_dll.lib` |
| 2 | **maya_nif_plugin** | `Release - 2025\nifTranslator.mll` |

> niflib builds automatically as a project dependency — no manual steps needed.
> Just make sure the `niflib/` submodule is populated from [Step 1](#step-1--clone-the-repository).

**What the final plugin links against:**

| Library | Type |
|:--------|:-----|
| `nifly.lib` | Static — baked into the `.mll` |
| `niflib_dll.lib` | Import lib for the niflib DLL |
| `Foundation.lib`, `OpenMaya.lib`, `OpenMayaAnim.lib`, `OpenMayaUI.lib`, `OpenMayaFX.lib`, `OpenMayaRender.lib`, `Image.lib` | Maya SDK *(bundled in repo)* |

---

### Step 4 — Deploy to Maya

The **post-build step runs automatically** on a successful build and copies
everything to the correct Maya user directories.

#### Plug-ins → `%USERPROFILE%\Documents\maya\2025\plug-ins\`

| File | Purpose |
|:-----|:--------|
| **`nifTranslator.mll`** | The plugin itself |
| **`niflib_x64.dll`** | Runtime library *(must be next to the `.mll`)* |

#### Scripts → `%USERPROFILE%\Documents\maya\2025\scripts\`

| File | Purpose |
|:-----|:--------|
| `nifTranslatorOpts.mel` | Import / export options dialog |
| `nifTranslatorMenuCreate.mel` | Creates the **NifTools** menu in Maya |
| `nifTranslatorMenuRemove.mel` | Removes the menu on plug-in unload |
| `AEbsLightningShaderTemplate.mel` | Attribute Editor template for BS Lightning Shader |
| `AEnifDismemberPartitionTemplate.mel` | Attribute Editor template for dismember partitions |

> [!NOTE]
> If the post-build step fails (different install paths, etc.), copy the files
> manually. The two **critical** files are `nifTranslator.mll` and
> `niflib_x64.dll` — both must be in Maya's `plug-ins` folder.

> [!TIP]
> **Targeting a different Maya version?** Change the folder name from `2025` to
> your version and compile against the matching devkit.

---

### Step 5 — Load the Plugin in Maya

1. Launch **Maya 2025**.
2. **Windows → Settings / Preferences → Plug-in Manager**.
3. Find **`nifTranslator.mll`** in the list.
4. Check **Loaded** *(and optionally* ***Auto load*** *for future sessions)*.
5. A **NifTools** menu should appear in the menu bar.
6. **File → Import** to open `.nif` / `.kf` files.
   **File → Export Selection** to export geometry to `.nif`.

---

---

## Troubleshooting

<details>
<summary><strong>"Cannot find niflib_x64.dll"</strong></summary>

Make sure `niflib_x64.dll` is in the **same folder** as `nifTranslator.mll`
(the `plug-ins` directory), or in a directory on your system `PATH`.

</details>

<details>
<summary><strong>"nifly.lib not found" — linker error</strong></summary>

Build nifly first ([Step 2](#step-2--build-nifly-static-library)) and place
`nifly.lib` where the project expects it. Check `AdditionalLibraryDirectories`
in the `Release - 2025` config of `maya_nif_plugin.vcxproj` for the exact path.

</details>

<details>
<summary><strong>"Cannot open include file: maya/MFnPlugin.h"</strong></summary>

The Maya SDK headers should already be in `extern/maya2025/include/maya/`. If
the folder is missing or empty, make sure you have the latest version of the
repo. The headers are committed directly — no devkit download needed.

</details>

<details>
<summary><strong>Plugin loads but no NifTools menu appears</strong></summary>

The MEL scripts are missing from your Maya scripts directory.
Copy them manually from the repo root to
`%USERPROFILE%\Documents\maya\2025\scripts\`:

- `nifTranslatorOpts.mel`
- `nifTranslatorMenuCreate.mel`
- `nifTranslatorMenuRemove.mel`

</details>

<details>
<summary><strong>Import produces an empty scene</strong></summary>

Check the debug log at:

```
%USERPROFILE%\Documents\maya\2025\scripts\nifTranslator_debug.log
```

Common causes:
- The NIF is pre-Skyrim LE and falls through to the legacy niflib path.
- The file is corrupt or uses an unsupported NIF version.

</details>

---

## Project Structure

```
maya_nif_plugin/
│
├── NifTranslator.cpp / .h              Main Maya translator entry point
├── maya_nif_plugin.sln                  Visual Studio solution
├── maya_nif_plugin.vcxproj              Visual Studio project (all build configs)
│
├── niflib/                              Submodule ─ niflib DLL (legacy NIF I/O)
├── extern/
│   ├── nifly/                           Submodule ─ nifly static lib (modern NIF I/O)
│   └── maya2025/                        Bundled Maya 2025 SDK (headers + import libs)
│       ├── include/maya/                541 Maya API headers
│       └── lib/                         Foundation, OpenMaya, OpenMayaAnim, etc.
│
├── include/
│   ├── Common/                          Shared types, options, utilities
│   ├── Custom Nodes/                    BSLightningShader, NifDismemberPartition
│   ├── Exporters/                       Export fixtures (Default, Skyrim, Nifly, KF)
│   └── Importers/                       Import fixtures (Default, Skyrim, Nifly, KF)
│
├── src/                                 Implementation files — mirrors include/ layout
│
├── nifTranslatorOpts.mel                Import / export options dialog
├── nifTranslatorMenuCreate.mel          Creates the NifTools menu
├── nifTranslatorMenuRemove.mel          Removes the NifTools menu
├── AEbsLightningShaderTemplate.mel      AE template ─ BS Lightning Shader
├── AEnifDismemberPartitionTemplate.mel  AE template ─ Dismember Partition
│
└── README.md                            This file
```

---

## Original Project

The original Maya NIF File Translator by Alecu100 was documented at:
<http://www.niftools.org/wiki/index.php/Maya>
