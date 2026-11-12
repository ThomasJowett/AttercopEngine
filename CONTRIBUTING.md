# Contributing to Attercop

This document provides foundational mandates and architectural guidance for working on Attercop. Adhere to these instructions to ensure consistency and maintainability across the codebase.

## 🚀 Project Overview
- **Language**: C++ 17
- **Build System**: CMake
- **Platforms**: Windows, macOS, Linux
- **Renderer**: Dual-API support for OpenGL and WebGPU.
- **ECS**: EnTT (Entity Component System).
- **Scripting**: Lua integrated via Sol2.

## 🧭 Principles
- **Fast to load**: the editor and games start quickly. Don't add work to start-up that can be deferred.
- **Work from source**: the engine is meant to be built and changed directly, not used as a black box.
- **Readable files**: project files are human readable and can be edited by hand. Binary is only for data where it matters for performance (e.g. mesh vertex data), and even then any references and the version go in a readable header.
- **Assets are identified by their file path**: no GUIDs, no `.meta` or sidecar files. Renaming or moving an asset in the editor updates every reference to it.
- **Deleting an asset deletes the file**: no redirects or hidden leftovers. References to it are reported as missing.
- **Importers just work**: no import options. Support a few formats well rather than many formats badly: glTF for models, Tiled for tilemaps.

## 🛠 Coding Standards & Conventions

### Memory Management
- **NEVER** use `std::shared_ptr` or `std::unique_ptr` directly for high-level engine objects.
- **ALWAYS** use the following custom aliases defined in `Core/core.h`:
  - `Scope<T>` instead of `std::unique_ptr<T>`.
  - `Ref<T>` instead of `std::shared_ptr<T>`.
- Use the helper functions for instantiation:
  - `CreateScope<T>(...)`
  - `CreateRef<T>(...)`

### Formatting
- **Indentation**: Use **Tabs** for all C++ files.
- **PCH**: While the project has `stdafx.h`, it is **NOT** mandatory to include it first in every `.cpp` file.

### Naming Conventions
- **Files**: PascalCase (e.g., `Application.cpp`, `RendererAPI.h`).
- **Classes & Methods**: PascalCase (e.g., `class Scene`, `void OnUpdate()`).
- **Variables**: camelCase (e.g., `float deltaTime`).
- **Member Variables**: Prefix with `m_` (e.g., `m_IsRunning`).
- **Static Variables**: Prefix with `s_` (e.g., `s_Instance`).

### Logging
Use the provided spdlog-based macros for all logging:
- **Engine-side**: `ENGINE_TRACE`, `ENGINE_DEBUG`, `ENGINE_INFO`, `ENGINE_WARN`, `ENGINE_ERROR`, `ENGINE_CRITICAL`.
- **Client/Application-side**: `CLIENT_TRACE`, `CLIENT_DEBUG`, `CLIENT_INFO`, `CLIENT_WARN`, `CLIENT_ERROR`, `CLIENT_CRITICAL`.

### Error Handling & Assertions
- Use `CORE_ASSERT(condition, message)` for engine-level assertions.
- Use `ASSERT(condition, message)` for client-level assertions.
- For fatal crashes, use `ENGINE_CRITICAL` followed by `DEBUGBREAK()`.

### Events
- The engine uses a custom Event system.
- Use `BIND_EVENT_FN(method)` macro to bind member functions to event callbacks.

## 🏗 Architecture Details

### Application & Layers
- The `Application` class manages a `LayerStack`.
- Functionality should be encapsulated in `Layer` or `Overlay` subclasses.
- Common entry points:
  - `Editor`: Main workspace for development.
  - `Runtime`: Minimal wrapper for running shipped games.

### Rendering
- Abstracted through `RendererAPI`.
- Platform-specific implementations reside in `Engine/src/Platform/OpenGL` and `Engine/src/Platform/WebGPU`.
- Use `Renderer2D` for sprite, text, and primitive rendering.

### Shaders
- Shaders are written once in Slang in `Engine/shaders` (shared modules in `Engine/shaders/Common`) and listed in `Engine/shaders/CMakeLists.txt` with the backends they're built for.
- The build compiles them into GLSL (`.vert`/`.frag`) and WGSL in `<build>/Shaders` and copies them into each executable's `data/Shaders`. They aren't committed. An executable that loads shaders calls `attercop_target_shaders(<target>)`.
- `slangc` is downloaded by CMake at configure time, nothing has to be installed. Set `SLANGC` to use a different one.
- Binding numbers live in `Engine/src/Renderer/ShaderBindings.h`, shared by the shaders and the C++ that binds to them. A texture at binding N has its sampler at N+1 on WebGPU; on OpenGL, N is the texture unit.
- Use combined samplers (`Sampler2D`). Only vertex and fragment (and later compute) stages, since WGSL has no geometry or tessellation shaders.

### Scripting
- Component bindings are declared in the component's own header with the `REFLECT_LUA_*` macros (see `Engine/src/Scripting/Lua/LuaBindings.h`); global tables are bound in `Engine/src/Scripting/Lua/`.
- Components registered in `EnTT` should also be registered in Lua if they need to be accessible from scripts.
- **Lua names**: properties and functions are PascalCase (e.g. `body.GravityScale`, `body:ApplyImpulse()`). `REFLECT_LUA_PROPERTY` capitalises the C++ member name automatically. The exception is the components properties that are a single character, which stay lowercase like the C++ fields (`v.x`, `colour.r`).

## 📁 Directory Structure
- `Engine/`: Core engine source and vendor libraries.
- `Editor/`: Editor-specific logic and UI (ImGui).
- `Runtime/`: Standalone game runtime.
- `Resources/`: Assets, shaders, and configurations.

### `Engine/src`
Put new code in the folder that matches what it does; `Core/` is only for the application itself.
- `Core/`: the application and its main loop (`Application`, `Layer`, `LayerStack`, `Window`), settings, command-line parsing, the engine version and basic types (`core.h`, `UUID`, `Colour`).
- `Input/`: keyboard, mouse and gamepad input, key and button codes, cursors, and the input action system.
- `Math/`: bounding boxes, frustums, random numbers, triangulation and other maths helpers.
- `Asset/`: asset types (textures, meshes, materials, fonts, sprite sheets, tilesets, audio clips, scripts), the asset base class, and the virtual file system for asset bundles.
- `Renderer/`: the renderer, `Renderer2D`, render pipeline, post-processing and the abstract GPU types (buffers, framebuffers, pipelines, textures).
- `Platform/`: backend implementations: `OpenGL/`, `WebGPU/`, `Null/` (headless) and `Web/` (browser-only code, only compiled in Emscripten builds).
- `Scene/`: scenes, entities, components, the scene graph, serialisation and the asset manager.
- `Physics/`: 2D physics, collision layers and physics debug drawing.
- `Scripting/`: Lua bindings and the Lua manager.
- `AI/`, `Animation/`, `Events/`, `Logging/`, `ImGui/`: behaviour trees and pathfinding, animation, the event system, logging and profiling, and Dear ImGui integration.
- `Utilities/`: helpers that don't fit a system above (file and string utilities, serialisation helpers, geometry generation).

## 🔧 Build Instructions
- Use CMake to generate project files for your preferred IDE or build tool.
- **New Files**: `.cpp` and `.h` files are picked up automatically by a CMake glob of each folder, but `Engine/CMakeLists.txt` lists the folders explicitly: when adding a new folder under `Engine/src`, add it to that list too.
- Ensure submodules are initialized: `git submodule update --init --recursive`.
- Define `DEBUG` or `RELEASE` configurations appropriately.

## 🧪 Tests
- **Unit tests** are in `Tests/UnitTests`, written with [doctest](https://github.com/doctest/doctest) (vendored in `Tests/UnitTests/vendor`). Build the `UnitTests` target, then run them from the build folder with `ctest -L unit --output-on-failure`, or run the `UnitTests` executable directly to use doctest's options (e.g. `UnitTests -tc="Text*"` for matching test cases).
- **New test file**: add a `.cpp` to `Tests/UnitTests` that includes `vendor/doctest.h`; the glob picks it up when CMake next runs. Tests must not need a window, a GPU or files outside the repository: make test data in the test (in memory or under `std::filesystem::temp_directory_path()`), or use a small file in the repository via `RepositoryPath` in `TestHelpers.h`. The logger and an `Application` are set up in `main.cpp`. CI builds the tests as a unity build, so helper names must be unique across test files; a namespace named after the file keeps them apart.
- **Components**: `ComponentFixture.h` gives every component non-default values, which the scene XML, play-mode snapshot and Lua tests check. A new component must be added there; a test fails until it is.
- **Scene tests** play every `*Test.scene` in `Tests/Scenes/Scenes` headless in `SceneTest`: `ctest -L scene --output-on-failure`.
- **Labels**: `unit` and `scene`. `ctest -L "scene|unit"` runs both; CI runs each label as its own step on every platform. Tests aren't built for Emscripten, or when `ATTERCOP_BUILD_SCENE_TESTS` is off.
