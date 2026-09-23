#include "sound_ambience_class.hpp"

#include "utility/scanf.hpp"

#include "../object.hpp"

#include "assets/odf/definition.hpp"

#include "utility/string_icompare.hpp"
#include "utility/string_ops.hpp"

#include <charconv>

using we::string::iequals;

namespace we::world {

sound_ambience_class::sound_ambience_class(const assets::odf::definition& definition) noexcept
{
   for (const assets::odf::property& prop : definition.properties) {
      if (iequals("MinDistance", prop.key)) {
         scan(prop.value, _min_distance);
      }
      else if (iequals("MaxDistance", prop.key)) {
         scan(prop.value, _max_distance);
      }
#if 0 // Adding this just in case we start previewing sounds in the editor. 
      else if (iequals("Sound", prop.key) or iequals("SoundStream", prop.key)) {
      }
#endif
   }

   for (int i = 0; i < std::ssize(definition.instance_properties); ++i) {
      const assets::odf::property& prop = definition.properties[i];

      if (iequals("MinDistance", prop.key)) {
         _min_distance_instance_property_index = i;
      }
      else if (iequals("MaxDistance", prop.key)) {
         _max_distance_instance_property_index = i;
      }
   }
}

auto sound_ambience_class::get_min_distance(const object& object) const noexcept -> float
{
   if (_min_distance_instance_property_index > 0) {
      if (_min_distance_instance_property_index <
          std::ssize(object.instance_properties)) {

         const std::string_view value = string::trim_leading_whitespace(
            object.instance_properties[_min_distance_instance_property_index].value);

         if (float result;
             std::from_chars(value.data(), value.data() + value.size(), result).ec ==
             std::errc{}) {
            return result;
         }
      }
   }

   return _min_distance;
}

auto sound_ambience_class::get_min_distance() const noexcept -> float
{
   return _min_distance;
}

auto sound_ambience_class::get_max_distance(const object& object) const noexcept -> float
{
   if (_max_distance_instance_property_index > 0) {
      if (_max_distance_instance_property_index <
          std::ssize(object.instance_properties)) {

         const std::string_view value = string::trim_leading_whitespace(
            object.instance_properties[_max_distance_instance_property_index].value);

         if (float result;
             std::from_chars(value.data(), value.data() + value.size(), result).ec ==
             std::errc{}) {
            return result;
         }
      }
   }

   return _max_distance;
}

auto sound_ambience_class::get_max_distance() const noexcept -> float
{
   return _min_distance;
}

}
