# Getting Started

## Getting the Editor

There are three ways to get the Editor:

- **In your browser:** open the [web Editor](https://thomasjowett.github.io/AttercopEngine/editor/)
  to use Attercop without installing anything. It needs a browser with WebGPU, such as Chrome or
  Edge. It works on a project folder on your computer and exports games for the web; in Chrome
  and Edge it saves straight to that folder, and in other browsers you keep your changes with
  **File > Download Project**.
- **Download it:** builds for Windows (x64), macOS and Linux (x64 and arm64) are on the
  [Releases](https://github.com/ThomasJowett/AttercopEngine/releases) page. Unzip the build and
  run the Editor from the folder.
- **Build it from source:** see the
  [repo README](https://github.com/ThomasJowett/AttercopEngine#getting-started) for the
  prerequisites and build steps.

!!! note
    The downloads aren't code signed, so Windows and macOS warn about them the first time you open them:

    - **Windows:** on "Windows protected your PC", choose **More info**, then **Run anyway**.
    - **macOS:** open the app once, then choose **Open Anyway** in
      **System Settings > Privacy & Security**.

## Creating a project

Open the Editor. If no project is open, you'll land on the projects start screen; otherwise use
**File > New Project** (`Ctrl+Shift+N`) to create one, or **File > Open Project...** (`Ctrl+O`)
to open an existing one.

A project is a folder on disk containing a `.proj` file, your scenes, and your assets (sprites,
meshes, materials, scripts, and so on) - there's no separate "import" step that copies files
elsewhere, you work directly with the folder.

## Building a scene

Add entities via the Hierarchy panel, and attach/edit components (Sprite, Transform, Camera,
Lua Script, ...) via the Properties panel. Drag assets from the Content Explorer into the
viewport or onto a component field to assign them. See the
[Lua API Reference](LuaAPI/index.md) for scripting an entity's behaviour, and
[Lua Scripting](lua-scripting.md) for worked examples.

## Exporting a game

**File > Export Game** packages your project into a standalone, shippable executable - it
discovers every asset your scenes actually reference, lets you review what's about to be
included, and packs it all (scripts, textures, meshes, shaders, and so on) into the exported
build. The exported game runs independently of the Editor; players don't need the Editor
installed at all.

!!! tip
    Assets referenced only through a sprite atlas or a Material/Button/Tileset that also uses
    them are handled automatically - you don't need to hunt down what's actually used before
    exporting.
