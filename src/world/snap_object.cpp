#include "snap_object.hpp"
#include "math/quaternion_funcs.hpp"
#include "math/vector_funcs.hpp"

namespace we::world {

namespace {

auto get_transformed_corners(const object& object, const float3 object_position,
                             const object_class& object_class)
   -> std::array<float3, 8>
{
   std::array<float3, 8> snapping_corners =
      math::to_corners(object_class.model->bounding_box);

   for (auto& v : snapping_corners) {
      v = object.rotation * v + object_position;
   }

   return snapping_corners;
}

}

auto get_snapped_position(
   const object& snapping_object, const float3 snapping_position,
   const std::span<const object> world_objects, const float snap_radius,
   const absl::flat_hash_map<lowercase_string, object_class>& object_classes)
   -> std::optional<float3>
{
   if (not object_classes.contains(snapping_object.class_name)) {
      return std::nullopt;
   }

   const std::array<float3, 8> snapping_corners =
      get_transformed_corners(snapping_object, snapping_position,
                              object_classes.at(snapping_object.class_name));

   float3 closest_corner;
   float closest_distance = FLT_MAX;
   uint32 closest_index = 0;

   for (const auto& object : world_objects) {
      if (auto object_class_it = object_classes.find(object.class_name);
          object_class_it != object_classes.end()) {
         const std::array<float3, 8> object_corners =
            get_transformed_corners(object, object.position, object_class_it->second);

         for (const auto& corner : object_corners) {
            for (uint32 i = 0; i < snapping_corners.size(); ++i) {
               const float corner_distance = distance(corner, snapping_corners[i]);

               if (corner_distance < closest_distance) {
                  closest_corner = corner;
                  closest_distance = corner_distance;
                  closest_index = i;
               }
            }
         }
      }
   }

   if (closest_distance > snap_radius) return std::nullopt;

   float3 direction = normalize(closest_corner - snapping_corners[closest_index]);

   return snapping_position + direction * closest_distance;
}

}