struct MyUniform {
	color: vec4f,
	time: f32,
};

@group(0) @binding(0) var<uniform> uMyUniform: MyUniform;

struct VertexInput {
	@location(0) position: vec2f,
	@location(1) color: vec3f,
}

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
}

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var offset = vec2f(-0.6875, -0.463);
    offset += 0.3 * vec2f(cos(uMyUniform.time), sin(uMyUniform.time));
    
	var out: VertexOutput;
	out.position = vec4f(in.position.x + offset.x, in.position.y + offset.y, 0.0, 1.0);
	out.color = in.color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let color = in.color * uMyUniform.color.rgb;
    let linear_color = pow(in.color, vec3f(2.2));
	return vec4f(linear_color, 1.0);
} 