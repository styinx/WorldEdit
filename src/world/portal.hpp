#pragma once

#include "id.hpp"
#include "types.hpp"

#include <string>

namespace we::world {

struct portal {
   std::string name;

   quaternion rotation = {1.0f, 0.0f, 0.0f, 0.0f};
   float3 position{};

   float width = 8.0f;
   float height = 8.0f;

   std::string sector1;
   std::string sector2;

   id<portal> id{};

   bool operator==(const portal&) const noexcept = default;
};

using portal_id = id<portal>;

}
