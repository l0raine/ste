
#type compute
#version 450

layout(local_size_x = 128) in;

#include "girenderer_transform_buffer.glsl"
#include "indirect.glsl"

#include "light.glsl"
#include "light_cascades.glsl"
#include "shadow_drawid_to_lightid_ttl.glsl"

#include "intersection.glsl"

#include "mesh_descriptor.glsl"

layout(std430, binding = 14) restrict readonly buffer mesh_data {
	mesh_descriptor mesh_descriptor_buffer[];
};
layout(std430, binding = 15) restrict readonly buffer mesh_draw_params_data {
	mesh_draw_params mesh_draw_params_buffer[];
};

layout(std430, binding = 2) restrict readonly buffer light_data {
	light_descriptor light_buffer[];
};

layout(shared, binding = 4) restrict readonly buffer ll_counter_data {
	uint ll_counter;
};
layout(shared, binding = 5) restrict readonly buffer ll_data {
	uint ll[];
};
layout(shared, binding = 6) restrict readonly buffer directional_lights_cascades_data {
	light_cascade_descriptor directional_lights_cascades[];
};

layout(binding = 0) uniform atomic_uint counter;
layout(std430, binding = 10) restrict writeonly buffer idb_data {
	IndirectMultiDrawElementsCommand idb[];
};
layout(std430, binding = 0) restrict writeonly buffer sidb_data {
	IndirectMultiDrawElementsCommand sidb[];
};
layout(std430, binding = 1) restrict writeonly buffer dsidb_data {
	IndirectMultiDrawElementsCommand dsidb[];
};
layout(shared, binding = 8) restrict writeonly buffer drawid_to_lightid_ttl_data {
	drawid_to_lightid_ttl ttl[];
};
layout(shared, binding = 9) restrict writeonly buffer d_drawid_to_lightid_ttl_data {
	d_drawid_to_lightid_ttl d_ttl[];
};

uniform float cascades_depths[directional_light_cascades];

void main() {
	int draw_id = int(gl_GlobalInvocationID.x);
	if (draw_id >= mesh_descriptor_buffer.length())
		return;

	mesh_descriptor md = mesh_descriptor_buffer[draw_id];

	vec3 center_world = transform_model(md, md.bounding_sphere.xyz);
	vec3 center = transform_view(center_world);
	float radius = md.bounding_sphere.w;
	
	uint shadow_instance_count = 0;
	uint dir_shadow_instance_count = 0;

	// Check if the geometry intersects some light's effective range
	for (int i = 0; i < ll_counter; ++i) {
		uint light_idx = ll[i];
		light_descriptor ld = light_buffer[light_idx];
		vec3 l = ld.transformed_position;
		
		if (ld.type == LightTypeDirectional) {
			// For directional lights check if the geometry is in one of the cascades
			uint cascade_idx = light_get_cascade_descriptor_idx(ld);
			light_cascade_descriptor cascade_descriptor = directional_lights_cascades[cascade_idx];

			for (int cascade=0; cascade<directional_light_cascades; ++cascade) {
				// Read cascade parameters and construct projection matrix
				float cascade_proj_far;
				vec2 recp_viewport;
				mat3x4 M = light_cascade_projection(cascade_descriptor, cascade, l, cascades_depths, recp_viewport, cascade_proj_far);
				
				// Project the geometry bounding sphere into cascade-space.
				// Check that it intersects the viewport and is in front of the far-plane of the cascade
				vec3 center_in_cascade_space  = vec4(center, 1) * M;
				if (any(lessThan(abs(center_in_cascade_space.xy), vec2(1.f) + radius * recp_viewport)) &&
					center_in_cascade_space.z > -cascade_proj_far - radius) {
					d_ttl[draw_id].entries[dir_shadow_instance_count] = create_drawid_ttl_entry(uint(i), light_idx);
					++dir_shadow_instance_count;
					break;
				}
			}
		}
		else {
			// Check if the light effective range sphere intersects the geometry bounding sphere
			float lr = ld.effective_range;

			if (collision_sphere_sphere(l, lr, center, radius)) {
				ttl[draw_id].entries[shadow_instance_count] = create_drawid_ttl_entry(uint(i), light_idx);
				++shadow_instance_count;
			}
		}
	}

	// Write to scene and shadows indirect draw buffers
	if (shadow_instance_count > 0) {
		uint idx = atomicCounterIncrement(counter);

		IndirectMultiDrawElementsCommand c;
		c.count = mesh_draw_params_buffer[draw_id].count;
		c.instance_count = 1;
		c.first_index = mesh_draw_params_buffer[draw_id].first_index;
		c.base_vertex = mesh_draw_params_buffer[draw_id].base_vertex;
		c.base_instance = draw_id;

		idb[idx] = c;

		c.instance_count = shadow_instance_count;
		sidb[idx] = c;

		c.instance_count = dir_shadow_instance_count;
		dsidb[idx] = c;
	}
}
