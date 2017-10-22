
#include <common.glsl>
#include <voxels.glsl>


layout(std430, set=1, binding=0) restrict readonly buffer voxel_buffer_binding {
	uint voxel_buffer[];
};


struct voxel_traversal_result_t {
	float distance;
	uint hit_voxel;
};

/**
*	@brief	Traverses a ray from V in direction dir, returning the first hit voxel, if any.
*/
voxel_traversal_result_t voxel_traverse(vec3 V, vec3 dir) {
	const vec3 edge = mix(vec3(.0f), vec3(1.f), greaterThan(dir, vec3(.0f)));
	const vec3 recp_dir = 1.f / dir;
	const vec3 sign_dir = sign(dir);
	const bvec3 b_dir_positive = equal(sign_dir, vec3(1));

	const uint n = voxel_leaf_level - 1;
	const float grid_res = float(voxel_resolution(n));
	const float grid_res_0 = float(voxel_resolution(0));
	const vec2 res_step = vec2(float(1 << voxel_P), 1.f / float(1 << voxel_P));

	const int full_mask = 0xFFFFFFFF;

	// Compute voxel world positions
	vec3 v = V / voxel_world + .5f;
//	if (any(lessThan(v, vec3(0))) || any(greaterThanEqual(v, vec3(1)))) {
//		// Outside voxel volume
//		const vec3 t = recp_dir * sign_dir * max(sign_dir * (edge - v), vec3(1e-6f));
//		const float t_bar = min_element(t) + 1e-5f;
//		v += dir * t_bar;
//	}

	// Init stack
	uint stack[voxel_leaf_level];
	stack[0] = voxel_root_node;

	// Traverse
	uint node = stack[0];
	uint P = voxel_Pi;
	int level_resolution = int(voxel_P * n);
	vec2 res = vec2(grid_res_0, 1.f / grid_res_0);

	ivec3 u = ivec3(v * grid_res);
	ivec3 b = (u >> level_resolution) % int(1 << voxel_Pi);

	int level = 0;
	while (true) {
		// Calculate brick coordinates
		const vec3 f = fma(v, res.xxx, -vec3(u >> level_resolution));
		const uint brick_idx = voxel_brick_index(b, P);

		// Compute binary map and pointer addresses
		const uvec2 binary_map_address = voxel_binary_map_address(brick_idx);
		const uint binary_map_word_ptr = node + voxel_node_binary_map_offset(P) + binary_map_address.x;

		// Check if we have child here
		const bool has_child = ((voxel_buffer[binary_map_word_ptr] >> binary_map_address.y) & 0x1) == 1;
		if (has_child) {
			// Step in
			++level;
			if (level == voxel_leaf_level)
				break;

			// Read child node address
			const uint child_ptr = node + voxel_node_children_offset(P) + brick_idx;

			// Read new level parameters
			P = voxel_P;
			node = voxel_buffer[child_ptr];
			level_resolution -= int(voxel_P);
			res *= res_step;

			b = (u >> level_resolution) % int(1 << voxel_P);

			// Update stack
			stack[level] = node;

			continue;
		}

		// No child, traverse.
		const vec3 t = recp_dir * sign_dir * max((edge - f) * sign_dir, vec3(1e-30f)); 		// Avoid nan created by division of 0 by inf
		const float t_bar = min_element(t);
		const bvec3 mixer = equal(vec3(t_bar), t);
		const vec3 step = max(t_bar, 1e-3f) * dir;

		// Step
		v += step * res.y;

		const int mask = 1 << level_resolution;
		const ivec3 u_hat = bitfieldInsert(u, full_mask.xxx, 0, level_resolution) + mix(-ivec3(mask), ivec3(1), b_dir_positive);
		const ivec3 u_bar = mix(ivec3(v * grid_res), u_hat, mixer);
		ivec3 b_offset = (u_bar >> level_resolution) - (u >> level_resolution);
		ivec3 b_bar = b + b_offset;

		while (any(lessThan(b_bar, ivec3(0))) ||
			   any(greaterThanEqual(b_bar, ivec3(1 << P)))) {
			// Step out
			--level;
			if (level < 0) {
				// No voxel was hit
				voxel_traversal_result_t ret;
				ret.distance = +inf;
				return ret;
			}

			// Pop stack
			node = stack[level];
			P = voxel_block_power(level);
			level_resolution += int(voxel_P);
			res *= res_step.yx;

			const ivec3 u_shift = u >> level_resolution;
			b_offset = (u_bar >> level_resolution) - u_shift;
			b_bar = u_shift % int(1 << P) + b_offset;
		}

		u = u_bar;
		b = b_bar;
	}
	
	if (level == voxel_leaf_level) {
		// Hit
		const vec3 pos = (v - .5f) * voxel_world;

		voxel_traversal_result_t ret;
		ret.hit_voxel = node;
		ret.distance = length(pos - V);

		return ret;
	}
	
	// No hit
	voxel_traversal_result_t ret;
	ret.distance = +inf;
	return ret;
}
