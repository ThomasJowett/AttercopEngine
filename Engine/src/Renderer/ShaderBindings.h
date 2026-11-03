#pragma once

// Binding numbers shared by the shaders in Engine/shaders and the code that binds resources to them.
// A texture at N has its sampler at N+1 on WebGPU. On OpenGL, N is the texture unit.

#define BINDING_CAMERA 0
#define BINDING_MODEL 1
#define BINDING_ALBEDO 2

#define BINDING_TEXTURE_SLOTS 1 // TEXTURE_SLOT_COUNT textures at 1, 3, 5...
#define TEXTURE_SLOT_COUNT 8

// Full-screen passes
#define BINDING_INPUT_TEXTURE 0
#define BINDING_ENTITY_ID 2
#define BINDING_POST_PROCESS 3
