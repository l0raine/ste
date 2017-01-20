
#include "stdafx.hpp"
#include "hdr_bokeh_blur_task.hpp"

using namespace StE::Graphics;
using namespace StE::Core;

void hdr_bokeh_blur_task::set_context_state() const {
	2_storage_idx = p->hdr_bokeh_param_buffer;

	screen_filling_quad.vao()->bind();
	p->bokeh_blur.get().bind();
}

void hdr_bokeh_blur_task::dispatch() const {
	Core::GL::gl_current_context::get()->draw_arrays(GL_TRIANGLE_STRIP, 0, 4);
}
