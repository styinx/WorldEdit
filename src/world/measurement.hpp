#pragma once

#include "id.hpp"
#include "types.hpp"

#include "math/bounding_box.hpp"

#include <string>
#include <vector>

namespace we::world {

struct measurement {
   bool hidden = false;

   std::vector<float3> points;

   std::string name;

   id<measurement> id{};

   bool operator==(const measurement&) const noexcept = default;
};

using measurement_id = id<measurement>;

}
