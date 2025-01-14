#pragma once

#include "graphics/camera.hpp"
#include "key.hpp"
#include "types.hpp"
#include "world/tool_visualizers.hpp"

#include <optional>

namespace we {

struct gizmo {
   bool want_capture_mouse() const noexcept;

   void update(const graphics::camera_ray cursor_ray, const bool is_mouse_down) noexcept;

   void draw(world::tool_visualizers& tool_visualizers) noexcept;

   bool show_translate(const float3 gizmo_position, float3& movement) noexcept;

   bool show_rotate(const float3 gizmo_position, float3& rotation) noexcept;

private:
   auto get_translate_position(const graphics::camera_ray ray,
                               const float3 fallback) const noexcept -> float3;

   auto get_rotate_position(const graphics::camera_ray ray,
                            const float3 fallback) const noexcept -> float3;

   enum class mode : uint8 { inactive, translate, rotate };
   enum class axis : uint8 { none, x, y, z };

   mode _last_mode = mode::inactive;
   mode _mode = mode::inactive;

   bool _used_last_tick = false;

   float3 _gizmo_position = {0.0f, 0.0f, 0.0f};

   struct translate_state {
      float3 start_position = {0.0f, 0.0f, 0.0f};
      float3 start_cursor_position = {0.0f, 0.0f, 0.0f};
      std::optional<float3> start_movement = std::nullopt;
      std::optional<float> movement_x;
      std::optional<float> movement_y;
      std::optional<float> movement_z;

      axis active_axis = axis::none;
      bool mouse_down_over_gizmo = false;
      bool translating = false;
   } _translate;

   struct rotate_state {
      float3 current_cursor_position = {0.0f, 0.0f, 0.0f};
      std::optional<float> angle_x;
      std::optional<float> angle_y;
      std::optional<float> angle_z;

      axis active_axis = axis::none;
      bool mouse_down_over_gizmo = false;
      bool rotating = false;
   } _rotate;

   float _translate_gizmo_size = 4.0f;
   float _translate_gizmo_hit_length = 0.25f;
   float _rotate_gizmo_radius = 4.0f;
   float _rotate_gizmo_hit_height = 0.25f;
   float _rotate_gizmo_hit_pad = 0.25f;
};

}