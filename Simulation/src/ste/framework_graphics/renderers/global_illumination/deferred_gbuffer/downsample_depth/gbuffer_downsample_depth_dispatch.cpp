
#include "stdafx.hpp"
#include "gbuffer_downsample_depth_dispatch.hpp"

using namespace StE::Graphics;

void gbuffer_downsample_depth_dispatch::set_context_state() const {
	using namespace Core;

	program.get().bind();
}

void gbuffer_downsample_depth_dispatch::dispatch() const {
	using namespace Core;

	constexpr int jobs = 32;

	auto size = gbuffer->get_size() / 2;

	for (int i = 0; i < gbuffer->get_downsampled_depth_target()->get_levels(); ++i, size /= 2) {
		4_image_idx = gbuffer->get_downsampled_depth_target()->make_image(i).with_access(image_access_mode::Write);
		program.get().set_uniform("lod", i);

		auto s = (size + glm::ivec2(jobs - 1)) / jobs;

		if (i != 1) Core::GL::gl_current_context::get()->memory_barrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		Core::GL::gl_current_context::get()->dispatch_compute(s.x, s.y, 1);
	}
}
