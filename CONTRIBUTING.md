# Contributing to Attercop

This document provides foundational mandates and architectural guidance for working on Attercop. Adhere to these instructions to ensure consistency and maintainability across the codebase.

## 🚀 Project Overview
- **Language**: C++ 17
- **Build System**: CMake
- **Platforms**: Windows, macOS, Linux
- **Renderer**: Dual-API support for OpenGL and WebGPU.
- **ECS**: EnTT (Entity Component System).
- **Scripting**: Lua integrated via Sol2.

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
