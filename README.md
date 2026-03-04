# Dust3D (redust3d fork)

> **This is a fork of [huxingyi/dust3d](https://github.com/huxingyi/dust3d) (RC9/master) that re-integrates the rigging system from RC6.**

Dust3D is a cross-platform 3D modeling software that makes it easy to create low poly 3D models for video games, 3D printing, and more.

## What this fork adds

The upstream RC9 codebase removed the rigging system that was present in RC6. This fork ports it back, adapted to RC9's architecture (no Qt types in the core library, no TBB, `Object` instead of `Outcome`).

### Rigging features

- **Bone marks** — right-click any node in the canvas to assign a bone mark (Neck, Limb, Tail, Joint). Marks guide the auto-rigger in identifying spine, limb, and tail chains.
- **Rig type menu** — the **Rig** menu in the menu bar lets you switch between *None* and *Animal* rigging modes.
- **Auto-rig generation** — selecting *Animal* triggers the `RigGenerator` which builds a skeleton from the bone-marked mesh automatically. The rig regenerates whenever the mesh changes.
- **FBX skeleton export** — exporting as FBX includes the generated skeleton (armature + limb nodes) and per-vertex skin weights so the model can be animated in external tools.

### Changed files (relative to upstream RC9)

| File | Change |
|------|--------|
| `dust3d/base/bone_mark.h/.cc` | Added `BoneMark` enum and string conversion |
| `dust3d/base/rig_type.h/.cc` | Added `RigType` enum (None, Animal) |
| `dust3d/base/rig_bone.h` | Added `RiggerBone`, `RiggerVertexWeights`, `BoneRole`, `BoneSide` |
| `dust3d/base/object.h` | Extended `ObjectNode` with `partId`, `nodeId`, `radius`, `boneMark`; added `bodyEdges` to `Object` |
| `dust3d/mesh/mesh_generator.cc/.h` | Populates `boneMark`, `bodyEdges`, and `triangleSourceNodes` after mesh generation |
| `application/sources/rig_generator.h/.cc` | Full port of RC6 `RigGenerator` to RC9 types (no Qt, no TBB) |
| `application/sources/document.h/.cc` | Wires rig generation lifecycle: `setRigType`, `setNodeBoneMark`, `regenerateRig`, `rigReady` |
| `application/sources/skeleton_graphics_widget.cc` | Bone mark submenu in canvas right-click context menu |
| `application/sources/document_window.cc` | Rig menu, connects signals, passes rig data to FBX exporter |
| `application/sources/fbx_file.h/.cc` | Generates FBX armature, limb nodes, skin deformer, and cluster weights |

## Getting Started

These instructions will get you a copy of the Dust3D up and running on your local machine for development and testing purposes.

### Prerequisites

In order to build and run Dust3D, you will need the following software and tools:

- Qt
- Visual Studio 2019 (Windows only)
- GCC (Linux only)

#### Windows

1. Download and install the `Qt Online Installer`
2. Run the installer and choose the Qt archives you want to install (required: qtbase, qtsvg)
3. Install Visual Studio 2019

#### Linux

1. Install Qt using your distribution's package manager (e.g. `apt-get install qt5-default libqt5svg5-dev` on Ubuntu)

### Building

A step by step series of examples that tell you how to get a development environment running

1. Clone the repository
```
git clone https://github.com/huxingyi/dust3d.git
```

2. Build using qmake

#### Windows

1. Open the Qt Creator IDE
2. Select "Open Project" from the File menu
3. Navigate to the project directory `dust3d/application` and open the `.pro` file
4. Select the desired build configuration (e.g. Debug or Release) from the dropdown menu at the bottom left of the window
5. Click the "Run" button to build and run the project

#### Linux

1. Change to the project directory  `dust3d/application`
2. Run `qmake` to generate a Makefile
3. Build the project using `make`

### Releasing

1. Make sure all changes are merged to master branch including CHANGELOGS update
2. Run `git tag 1.0.0-rc.<number>`
3. Run `git push origin 1.0.0-rc.<number>`
4. Wait Actions/release finish and download all the Artifacts
5. Goto `Tags/1.0.0-rc.<number>` and create release from tag
6. Title the release as `1.0.0-rc.<number>`
7. Copy description from CHANGELOGS
8. Drag the Artifacts: `dust3d-1.0.0-rc.<number>.zip`, `dust3d-1.0.0-rc.<number>.AppImage (Extracted)`, `dust3d-1.0.0-rc.<number>.dmg (Extracted)` to binaries.
9. Publish release

## License

Dust3D is licensed under the MIT License - see the [LICENSE](https://github.com/huxingyi/dust3d/blob/master/LICENSE) file for details.

<!-- Sponsors begin --><!-- Sponsors end -->