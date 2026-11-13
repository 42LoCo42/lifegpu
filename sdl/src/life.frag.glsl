#version 450

layout(location = 0) out vec4 color;

layout(set = 2, binding = 0) readonly buffer Src {
	uint src[];
};

layout(set = 3, binding = 0) uniform Params {
	uint   width;
	uint   height;
	double zoom;
};

void main() {
	const vec3 colors[4] = vec3[](
		vec3(0.0, 0.0, 0.0),
		vec3(1.0, 0.0, 0.0),
		vec3(1.0, 0.5, 0.0),
		vec3(1.0, 1.0, 0.0)
	);

	ivec2 px  = ivec2(gl_FragCoord.xy / zoom);
	uint  cur = src[uint(px.y) * width + uint(px.x)];
	color     = vec4(colors[cur], 1.0);
}
