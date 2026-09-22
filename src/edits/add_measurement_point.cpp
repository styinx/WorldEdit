#include "add_sector_object.hpp"

#include "add_vector_entry.hpp"

namespace we::edits {

auto make_add_measurement_point(std::vector<float3>* points, const float3& point)
   -> std::unique_ptr<edit<world::edit_context>>
{
   return make_add_vector_entry(points, point);
}

}