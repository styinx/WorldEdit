#pragma once

#include "../measurement.hpp"

#include "math/bounding_box.hpp"

#include <span>
#include <string_view>

namespace we::world {

struct measurement_metrics {
   float length = 0.0f;
   float3 centre = {};
   math::bounding_box bbox;
};

auto get_measurement_metrics(const measurement& measurement) noexcept
   -> measurement_metrics;

}