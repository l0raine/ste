// StE
// � Shlomi Steinberg, 2015

#pragma once

#include "task.h"
#include "Scene.h"
#include "Texture2D.h"

#include "StEngineControl.h"
#include "material_storage.h"

#include <memory>
#include <unordered_map>
#include <string>

#include <vector>
#include <future>

#include <tinyobjloader/tiny_obj_loader.h>

namespace StE {
namespace Resource {

class ModelLoader {
private:
	using texture_map_type = std::unordered_map<std::string, std::shared_ptr<LLR::Texture2D>>;
	using brdf_map_type = std::unordered_map<std::string, std::shared_ptr<Graphics::BRDF>>;
	using shapes_type = std::vector<tinyobj::shape_t>;
	using materials_type = std::vector<tinyobj::material_t>;

private:
	~ModelLoader() {}

	static StE::task<void> load_texture(const std::string &name, bool srgb, texture_map_type *texmap, bool bumpmap, const std::string &dir, float normal_map_bias);
	static std::vector<std::future<void>> load_textures(task_scheduler* sched, shapes_type &shapes, materials_type &materials, texture_map_type &tex_map, const std::string &dir, float normal_map_bias);
	static std::vector<std::future<void>> load_brdfs(const StEngineControl *context, shapes_type &shapes, materials_type &materials, brdf_map_type &brdf_map, const std::string &dir);
	static std::future<void> process_model_mesh(optional<task_scheduler*> sched,
												Graphics::material_storage *,
												const tinyobj::shape_t &,
												Graphics::Scene *,
												materials_type &,
												texture_map_type &,
												brdf_map_type &);

public:
	static task<bool> load_model_task(const StEngineControl &context, const std::string &file_path, Graphics::Scene *scene, float normal_map_bias);
};

}
}
