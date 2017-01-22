
#include "common.glsl"
#include "project.glsl"
#include "atmospherics.glsl"
#include "linked_light_lists.glsl"

const int volumetric_scattering_tile_size = lll_image_res_multiplier;
const int volumetric_scattering_depth_tiles = 256;

const float volumetric_scattering_ka = 0.03f;
const float volumetric_scattering_kb = 1.07f;

float volumetric_scattering_depth_for_tile(float t) {
	float z = -volumetric_scattering_ka * pow(volumetric_scattering_kb, t);
	return -1.f / z;
}

float volumetric_scattering_tile_for_depth(float d) {
	float z = -1.f / d;
	return log(-z / volumetric_scattering_ka) / log(volumetric_scattering_kb);
}

float volumetric_scattering_zcoord_for_depth(float d) {
	return volumetric_scattering_tile_for_depth(d) / float(volumetric_scattering_depth_tiles);
}

vec4 volumetric_scattering_load_inscattering_transmittance(sampler3D volume, vec2 frag_coords, float depth) {
	vec2 xy = frag_coords / (float(volumetric_scattering_tile_size) * textureSize(volume, 0).xy);
	vec3 p = vec3(xy, volumetric_scattering_zcoord_for_depth(depth));
	return texture(volume, p);
}

vec3 volumetric_scattering(sampler3D volume, vec2 frag_coords, float depth) {
	vec4 vol_sam = volumetric_scattering_load_inscattering_transmittance(volume,
																		 frag_coords,
																		 depth);
	return vol_sam.rgb;
}