
#include <stdafx.hpp>
#include <material_layer_descriptor.hpp>

using namespace StE::Graphics;

const float StE::Graphics::material_layer_max_ansio_ratio = 1.f / glm::sqrt(1.f - 1.f * material_layer_ansio_ratio_scale);
const float StE::Graphics::material_layer_min_ansio_ratio = glm::sqrt(1.f - 1.f * material_layer_ansio_ratio_scale);
