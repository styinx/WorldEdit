#pragma once

#include "assets/asset_libraries.hpp"
#include "assets/msh/flat_model.hpp"
#include "assets/odf/definition.hpp"
#include "lowercase_string.hpp"
#include "object_instance_property.hpp"

#include <memory>
#include <string>
#include <vector>

namespace we::world {

struct object_class : std::enable_shared_from_this<object_class> {
   object_class() = default;

   object_class(assets::libraries_manager& assets_libraries,
                asset_ref<assets::odf::definition> definition_asset);

   asset_ref<assets::odf::definition> definition_asset;
   asset_data<assets::odf::definition> definition;

   asset_ref<assets::msh::flat_model> model_asset;
   asset_data<assets::msh::flat_model> model;

   lowercase_string model_name;

   std::vector<instance_property> instance_properties;

   /// @brief Update the object class from a definition asset.
   /// @param assets_libraries A reference to the assets::libraries_manager to pull model assets from.
   /// @param new_definition_asset The new definition asset from the object class.
   void update_definition(assets::libraries_manager& assets_libraries,
                          asset_ref<assets::odf::definition> new_definition_asset);

   /// @brief Update the object class from it's current definition asset.
   /// @param assets_libraries A reference to the assets::libraries_manager to pull model assets from.
   void update_from_definition(assets::libraries_manager& assets_libraries);
};

}
