//	StE
// � Shlomi Steinberg 2015-2017

#pragma once

#include <ste_device.hpp>
#include <device_pipeline_shader_stage.hpp>
#include <pipeline_external_binding_set_impl.hpp>
#include <pipeline_external_binding_set.hpp>

#include <pipeline_layout_exceptions.hpp>

#include <lib/vector.hpp>
#include <lib/flat_map.hpp>
#include <lib/string.hpp>

#include <type_traits>
#include <optional.hpp>

namespace ste {
namespace gl {

/**
*	@brief	Extracts binding layout from a pipeline shader stage to generate an external binding set collection.
*/
class external_binding_set_collection_from_shader_stages {
	struct pipeline_external_binding_set_layout_descriptor {
		pipeline_binding_stages_collection stages;
		ste_shader_stage_binding binding;
	};

public:
	using shader_stages_input_vector_t = std::vector<std::pair<pipeline_binding_stages_collection, device_pipeline_shader_stage>>;

private:
	std::reference_wrapper<const ste_device> device;
	optional<pipeline_external_binding_set_layout> layout;

public:
	external_binding_set_collection_from_shader_stages(const ste_device &device,
													   shader_stages_input_vector_t &&shader_stages) : device(device)
	{
		if (!shader_stages.size()) {
			throw pipeline_layout_exception("No shader stages provided");
		}

		optional<pipeline_layout_set_index> set_idx;
		lib::flat_map<lib::string, pipeline_external_binding_set_layout_descriptor> bindings_map;
		for (auto &&stage_shader : shader_stages) {
			// Extract bindings out of shader object
			auto shader_object = std::move(stage_shader.second).shader_object();
			auto stages = std::move(stage_shader.first);

			for (auto &&binding : shader_object->stage_bindings) {
				auto &name = binding.variable->name();

				auto it = bindings_map.lower_bound(name);
				if (it != bindings_map.end() && it->first == name) {
					// Name exists
					if (it->second.binding != binding) {
						// Incompatible
						throw pipeline_layout_incompatible_binding_exception("Incompatible bindings with identical name");
					}
					it->second.stages.insert(stages);
				}
				else {
					if (!set_idx)
						set_idx = it->second.binding.set_idx;

					// Verify set index
					if (set_idx) {
						if (set_idx.get() != it->second.binding.set_idx) {
							throw pipeline_layout_exception("Provided shader stages contain variables bound to multiple sets. Only one set allowed.");
						}
					}

					// Insert new binding descriptor into map
					pipeline_external_binding_set_layout_descriptor descriptor = { std::move(stages), std::move(binding) };
					bindings_map.emplace_hint(it,
											  name,
											  std::move(descriptor));
				}
			}
		}

		// Generate the actual binding layouts and group them into sets
		lib::vector<pipeline_external_binding_layout> binding_layouts;
		binding_layouts.reserve(bindings_map.size());
		for (auto &&p : bindings_map) {
			pipeline_external_binding_layout binding_layout(p.first,
															std::move(p.second.stages),
															std::move(p.second.binding));
			binding_layouts.emplace_back(std::move(binding_layout));
		}

		// Create the external binding set layout
		layout.emplace(device.get(), 
					   std::move(binding_layouts),
					   set_idx.get());
	}

	auto generate() && {
		return pipeline_external_binding_set(std::move(layout.get()),
											 device.get().binding_set_pool());
	}
};

}
}
