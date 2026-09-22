#pragma once

#include "edit.hpp"

#include "world/interaction_context.hpp"

#include <memory>

namespace we::edits {

auto make_add_measurement_point(std::vector<float3>* points, const float3& point)
   -> std::unique_ptr<edit<world::edit_context>>;

}
