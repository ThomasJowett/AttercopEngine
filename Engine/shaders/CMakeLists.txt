# Every engine shader and the backends it's compiled for (see cmake/Slang.cmake)

attercop_add_shader(FinalPassSwapchain TARGETS glsl wgsl)
attercop_add_shader(Renderer2D_Quad TARGETS glsl wgsl)
attercop_add_shader(Renderer2D_Text TARGETS glsl wgsl)
attercop_add_shader(Renderer2D_Circle TARGETS glsl wgsl)
attercop_add_shader(Renderer2D_HairLine TARGETS glsl wgsl)
attercop_add_shader(Renderer2D_Line TARGETS glsl wgsl)
attercop_add_shader(Standard TARGETS glsl wgsl)
attercop_add_shader(Grid TARGETS glsl wgsl)
attercop_add_shader(BlurEffect TARGETS glsl wgsl)
attercop_add_shader(FinalPass TARGETS glsl wgsl)

# WebGPU framebuffer copies and multisample resolve
attercop_add_shader(DepthBlit TARGETS wgsl)
attercop_add_shader(EntityIdResolve TARGETS wgsl)

attercop_add_shader_target()
