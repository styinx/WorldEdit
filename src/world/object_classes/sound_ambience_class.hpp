#pragma once

namespace we::assets::odf {

struct definition;

}

namespace we::world {

struct object;

struct sound_ambience_class {
   explicit sound_ambience_class(const assets::odf::definition& definition) noexcept;

   auto get_min_distance(const object& object) const noexcept -> float;

   auto get_min_distance() const noexcept -> float;

   auto get_max_distance(const object& object) const noexcept -> float;

   auto get_max_distance() const noexcept -> float;

private:
   int _min_distance_instance_property_index = -1;
   int _max_distance_instance_property_index = -1;

   float _min_distance = 1.0f;
   float _max_distance = 10.0f;
};

}
