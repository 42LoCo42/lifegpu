#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) readonly buffer Src {
	uint src[];
};

layout(set = 1, binding = 0) buffer Dst {
	uint dst[];
};

layout(set = 2, binding = 0) uniform Params {
	uint width;
	uint height;
};

void main() {
	uint x = gl_GlobalInvocationID.x;
	uint y = gl_GlobalInvocationID.y;

	if(x >= width || y >= height) return;

	uint neighbours = 0;

	for(int dy = -1; dy <= 1; dy++) {
		for(int dx = -1; dx <= 1; dx++) {
			if(dx == 0 && dy == 0) continue;

			int  nx = (int(x) + dx + int(width)) % int(width);
			int  ny = (int(y) + dy + int(height)) % int(height);
			uint i  = uint(ny) * width + uint(nx);
			neighbours += (src[i] == 1) ? 1 : 0;
		}
	}

	uint pos = y * width + x;
	uint cur = src[pos];

	switch(cur) {
	case 0:
		dst[pos] = (neighbours == 2) ? 1 : 0;
		break;

	case 1:
		dst[pos] = (neighbours == 3 || neighbours == 4 || neighbours == 5) ? 1 : 2;
		break;

	default:
		dst[pos] = (cur + 1) % 4;
		break;
	}

	// switch(cur) {
	// case 0:
	// 	dst[pos] = (neighbours == 3) ? 1 : 0;
	// 	break;

	// case 1:
	// 	dst[pos] = (neighbours == 2 || neighbours == 3) ? 1 : 0;
	// }
}
