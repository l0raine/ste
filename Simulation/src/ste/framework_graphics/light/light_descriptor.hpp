// StE
// © Shlomi Steinberg, 2015-2017

#pragma once

#include "stdafx.hpp"
#include "light_type.hpp"

#include "texture_handle.hpp"

namespace StE {
namespace Graphics {

struct light_descriptor {
	glm::vec3		position;		float radius{ .0f };
	glm::vec3		emittance;		LightType type;

	float			effective_range{ .0f };
	float			directional_distance;
	std::uint32_t	normal_pack;
	std::uint32_t	polygonal_light_points_and_offset_or_cascade_idx{ 0 };

	Core::texture_handle texture;
	float _unsued0[2];

	float _internal[4];

public:
	void set_polygonal_light_points(std::uint8_t points, std::uint32_t offset) {
		polygonal_light_points_and_offset_or_cascade_idx = (points << 24) | (offset & 0x00FFFFFF);
	}

	auto get_polygonal_light_buffer_offset() const {
		return polygonal_light_points_and_offset_or_cascade_idx & 0x00FFFFFF;
	}

	auto get_polygonal_light_point_count() const {
		return polygonal_light_points_and_offset_or_cascade_idx >> 24;
	}
};

}
}
