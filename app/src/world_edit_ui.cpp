
#include "world_edit.hpp"

#include "edits/creation_entity_set.hpp"
#include "edits/imgui_ext.hpp"
#include "edits/set_value.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_ext.hpp"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_stdlib.h"
#include "utility/look_for.hpp"
#include "utility/overload.hpp"
#include "world/utility/snapping.hpp"
#include "world/utility/world_utilities.hpp"

#include <numbers>

#include <range/v3/view/enumerate.hpp>

using namespace std::literals;
using ranges::views::enumerate;

namespace we {

namespace {

struct placement_traits {
   bool has_new_path = false;
   bool has_placement_rotation = true;
   bool has_point_at = true;
   bool has_placement_mode = true;
   bool has_lock_axis = true;
   bool has_placement_alignment = true;
   bool has_placement_ground = true;
   bool has_node_placement_insert = false;
   bool has_resize_to = false;
   bool has_from_bbox = false;
};

auto surface_rotation_degrees(const float3 surface_normal,
                              const float fallback_angle) -> float
{
   if (surface_normal.x == 0.0f and surface_normal.z == 0.0f) {
      return fallback_angle;
   }

   const float2 direction = normalize(float2{surface_normal.x, surface_normal.z});

   const float angle =
      std::atan2(-direction.x, -direction.y) + std::numbers::pi_v<float>;

   return std::fmod(angle * 180.0f / std::numbers::pi_v<float>, 360.0f);
}

auto align_position_to_grid(const float2 position, const float alignment) -> float2
{
   return float2{std::round(position.x / alignment) * alignment,
                 std::round(position.y / alignment) * alignment};
}

auto align_position_to_grid(const float3 position, const float alignment) -> float3
{
   return float3{std::round(position.x / alignment) * alignment, position.y,
                 std::round(position.z / alignment) * alignment};
}

}

void world_edit::update_ui() noexcept
{
   ImGui_ImplWin32_NewFrame();
   ImGui::NewFrame();
   ImGui::ShowDemoWindow(nullptr);

   _tool_visualizers.clear();

   if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
         if (ImGui::MenuItem("Open Project")) open_project_with_picker();

         ImGui::Separator();

         const bool loaded_project = not _project_dir.empty();

         if (ImGui::BeginMenu("Load World", loaded_project)) {
            auto worlds_path = _project_dir / L"Worlds"sv;

            for (auto& known_world : _project_world_paths) {
               auto relative_path =
                  std::filesystem::relative(known_world, worlds_path);

               if (ImGui::MenuItem(relative_path.string().c_str())) {
                  load_world(known_world);
               }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Browse...")) load_world_with_picker();

            ImGui::EndMenu();
         }

         const bool loaded_world = not _world_path.empty();

         if (ImGui::MenuItem("Save World",
                             get_display_string(
                                _hotkeys.query_binding("", "save")))) {
            save_world(_world_path);
         }

         if (ImGui::MenuItem("Save World As...", nullptr, nullptr, loaded_world)) {
            save_world_with_picker();
         }

         ImGui::Separator();

         if (ImGui::MenuItem("Close World", nullptr, nullptr, loaded_world)) {
            close_world();
         }

         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) {
         if (ImGui::MenuItem("Undo",
                             get_display_string(
                                _hotkeys.query_binding("", "edit.undo")))) {
            _edit_stack_world.revert(_edit_context);
         }
         if (ImGui::MenuItem("Redo",
                             get_display_string(
                                _hotkeys.query_binding("", "edit.redo")))) {
            _edit_stack_world.reapply(_edit_context);
         }

         ImGui::Separator();

         ImGui::MenuItem("Cut", nullptr, nullptr, false);
         ImGui::MenuItem("Copy", nullptr, nullptr, false);
         ImGui::MenuItem("Paste", nullptr, nullptr, false);

         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Create")) {
         if (ImGui::MenuItem("Object")) {
            const world::object* base_object =
               world::find_entity(_world.objects, _entity_creation_context.last_object);

            world::object new_object;

            if (base_object) {
               new_object = *base_object;

               new_object.name =
                  world::create_unique_name(_world.objects, base_object->name);
               new_object.id = world::max_id;
            }
            else {
               new_object =
                  world::object{.name = "",
                                .class_name = lowercase_string{"com_bldg_controlzone"sv},
                                .id = world::max_id};
            }

            _edit_stack_world
               .apply(edits::make_creation_entity_set(std::move(new_object),
                                                      _interaction_targets.creation_entity),
                      _edit_context);
         }

         if (ImGui::MenuItem("Light")) {
            const world::light* base_light =
               world::find_entity(_world.lights, _entity_creation_context.last_light);

            world::light new_light;

            if (base_light) {
               new_light = *base_light;

               new_light.name =
                  world::create_unique_name(_world.lights, base_light->name);
               new_light.id = world::max_id;
            }
            else {
               new_light = world::light{.name = "", .id = world::max_id};
            }

            _edit_stack_world
               .apply(edits::make_creation_entity_set(std::move(new_light),
                                                      _interaction_targets.creation_entity),
                      _edit_context);
         }

         if (ImGui::MenuItem("Path")) {
            const world::path* base_path =
               world::find_entity(_world.paths, _entity_creation_context.last_path);

            _edit_stack_world
               .apply(edits::make_creation_entity_set(
                         world::path{.name = world::create_unique_name(
                                        _world.paths, base_path ? base_path->name : "Path 0"),
                                     .layer = base_path ? base_path->layer : 0,
                                     .nodes = {world::path::node{}},
                                     .id = world::max_id},
                         _interaction_targets.creation_entity),
                      _edit_context);
         }

         if (ImGui::MenuItem("Region")) {
            const world::region* base_region =
               world::find_entity(_world.regions, _entity_creation_context.last_region);

            world::region new_region;

            if (base_region) {
               new_region = *base_region;

               new_region.name =
                  world::create_unique_name(_world.regions, base_region->name);
               new_region.id = world::max_id;
            }
            else {
               new_region =
                  world::region{.name = world::create_unique_name(_world.lights, "Region0"),
                                .id = world::max_id};
            }

            _edit_stack_world
               .apply(edits::make_creation_entity_set(std::move(new_region),
                                                      _interaction_targets.creation_entity),
                      _edit_context);
         }

         if (ImGui::MenuItem("Sector")) {
            const world::sector* base_sector =
               world::find_entity(_world.sectors, _entity_creation_context.last_sector);

            _edit_stack_world
               .apply(edits::make_creation_entity_set(
                         world::sector{.name = world::create_unique_name(
                                          _world.sectors,
                                          base_sector ? base_sector->name : "Sector0"),
                                       .base = 0.0f,
                                       .height = 10.0f,
                                       .points = {{0.0f, 0.0f}},
                                       .id = world::max_id},
                         _interaction_targets.creation_entity),
                      _edit_context);
         }
#if 0
         if (ImGui::MenuItem("Portal")) _create.portal = true;
         if (ImGui::MenuItem("Barrier")) _create.barrier = true;
         if (ImGui::MenuItem("Planning Hub")) _create.planning_hub = true;
         if (ImGui::MenuItem("Planning Connection"))
            _create.planning_connection = true;
         if (ImGui::MenuItem("Boundary")) _create.object = true;
#endif
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Developer")) {
         if (ImGui::MenuItem("Reload Shaders")) {
            try {
               _renderer->reload_shaders();
            }
            catch (graphics::gpu::exception& e) {
               handle_gpu_error(e);
            }
         }

         ImGui::Selectable("Show GPU Profiler", &_settings.graphics.show_profiler);

         ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
   }

   ImGui::SetNextWindowPos({0.0f, 32.0f * _display_scale});
   ImGui::SetNextWindowSize({224.0f * _display_scale, 512.0f * _display_scale});

   ImGui::Begin("World Active Context", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);

   ImGui::TextUnformatted("Active Layers");
   ImGui::Separator();
   ImGui::BeginChild("Active Layers", ImVec2{0.0f, 208.0f * _display_scale});

   for (auto [i, layer] : enumerate(_world.layer_descriptions)) {
      if (ImGui::Selectable(layer.name.c_str(), _world_layers_draw_mask[i])) {
         _world_layers_draw_mask[i] = not _world_layers_draw_mask[i];
      }
   }

   ImGui::EndChild();

   ImGui::TextUnformatted("Active Entities");
   ImGui::Separator();
   ImGui::BeginChild("Active Entities", ImVec2{0.0f, 236.0f * _display_scale});

   if (ImGui::Selectable("Objects", _world_draw_mask.objects)) {
      _world_draw_mask.objects = not _world_draw_mask.objects;
   }

   if (ImGui::Selectable("Lights", _world_draw_mask.lights)) {
      _world_draw_mask.lights = not _world_draw_mask.lights;
   }

   if (ImGui::Selectable("Paths", _world_draw_mask.paths)) {
      _world_draw_mask.paths = not _world_draw_mask.paths;
   }

   if (ImGui::Selectable("Regions", _world_draw_mask.regions)) {
      _world_draw_mask.regions = not _world_draw_mask.regions;
   }

   if (ImGui::Selectable("Sectors", _world_draw_mask.sectors)) {
      _world_draw_mask.sectors = not _world_draw_mask.sectors;
   }

   if (ImGui::Selectable("Portals", _world_draw_mask.portals)) {
      _world_draw_mask.portals = not _world_draw_mask.portals;
   }

   if (ImGui::Selectable("Hintnodes", _world_draw_mask.hintnodes)) {
      _world_draw_mask.hintnodes = not _world_draw_mask.hintnodes;
   }

   if (ImGui::Selectable("Barriers", _world_draw_mask.barriers)) {
      _world_draw_mask.barriers = not _world_draw_mask.barriers;
   }

   if (ImGui::Selectable("Planning Hubs", _world_draw_mask.planning_hubs)) {
      _world_draw_mask.planning_hubs = not _world_draw_mask.planning_hubs;
   }

   if (ImGui::Selectable("Planning Connections", _world_draw_mask.planning_connections)) {
      _world_draw_mask.planning_connections = not _world_draw_mask.planning_connections;
   }

   if (ImGui::Selectable("Boundaries", _world_draw_mask.boundaries)) {
      _world_draw_mask.boundaries = not _world_draw_mask.boundaries;
   }

   ImGui::EndChild();

   ImGui::End();

   if (_hotkeys_show) {
      ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x, 32.0f * _display_scale},
                              ImGuiCond_Always, {1.0f, 0.0f});
      ImGui::SetNextWindowSizeConstraints({224.0f * _display_scale, -1.0f},
                                          {224.0f * _display_scale, -1.0f});

      ImGui::Begin("Hotkeys", &_hotkeys_show,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoBringToFrontOnFocus |
                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs |
                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration);

      ImGui::End();
   }

   if (not _interaction_targets.selection.empty()) {
      ImGui::SetNextWindowPos({232.0f * _display_scale, 32.0f * _display_scale},
                              ImGuiCond_Once, {0.0f, 0.0f});

      bool selection_open = true;

      ImGui::Begin("Selection", &selection_open, ImGuiWindowFlags_NoCollapse);

      std::visit(
         overload{
            [&](world::object_id id) {
               world::object* object =
                  look_for(_world.objects, [id](const world::object& object) {
                     return id == object.id;
                  });

               if (not object) return;

               ImGui::InputText("Name", object, &world::object::name,
                                &_edit_stack_world, &_edit_context);
               ImGui::InputTextAutoComplete(
                  "Class Name", object, &world::object::class_name,
                  &_edit_stack_world, &_edit_context, [&] {
                     std::array<std::string, 6> entries;
                     std::size_t matching_count = 0;

                     _asset_libraries.odfs.enumerate_known(
                        [&](const lowercase_string& asset) {
                           if (matching_count == entries.size()) return;
                           if (not asset.contains(object->class_name)) return;

                           entries[matching_count] = asset;

                           ++matching_count;
                        });

                     return entries;
                  });
               ImGui::LayerPick("Layer", object, &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::DragQuat("Rotation", object, &world::object::rotation,
                               &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Position", object, &world::object::position,
                                 &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::SliderInt("Team", object, &world::object::team,
                                &_edit_stack_world, &_edit_context, 0, 15, "%d",
                                ImGuiSliderFlags_AlwaysClamp);

               ImGui::Separator();

               for (auto [i, prop] : enumerate(object->instance_properties)) {
                  if (prop.key.contains("Path")) {
                     ImGui::InputKeyValueAutoComplete(
                        object, &world::object::instance_properties, i,
                        &_edit_stack_world, &_edit_context, [&] {
                           std::array<std::string, 6> entries;

                           std::size_t matching_count = 0;

                           for (auto& path : _world.paths) {
                              if (not path.name.contains(prop.value)) {
                                 continue;
                              }

                              entries[matching_count] = path.name;

                              ++matching_count;

                              if (matching_count == entries.size()) break;
                           }

                           return entries;
                        });
                  }
                  else if (prop.key.contains("Region")) {
                     ImGui::InputKeyValueAutoComplete(
                        object, &world::object::instance_properties, i,
                        &_edit_stack_world, &_edit_context, [&] {
                           std::array<std::string, 6> entries;

                           std::size_t matching_count = 0;

                           for (auto& region : _world.regions) {
                              if (not region.description.contains(prop.value)) {
                                 continue;
                              }

                              entries[matching_count] = region.description;

                              ++matching_count;

                              if (matching_count == entries.size()) break;
                           }

                           return entries;
                        });
                  }
                  else {
                     ImGui::InputKeyValue(object, &world::object::instance_properties,
                                          i, &_edit_stack_world, &_edit_context);
                  }
               }
            },
            [&](world::light_id id) {
               world::light* light =
                  look_for(_world.lights, [id](const world::light& light) {
                     return id == light.id;
                  });

               if (not light) return;

               ImGui::InputText("Name", light, &world::light::name,
                                &_edit_stack_world, &_edit_context);
               ImGui::LayerPick("Layer", light, &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::DragQuat("Rotation", light, &world::light::rotation,
                               &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Position", light, &world::light::position,
                                 &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::ColorEdit3("Color", light, &world::light::color,
                                 &_edit_stack_world, &_edit_context,
                                 ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

               ImGui::Checkbox("Static", light, &world::light::static_,
                               &_edit_stack_world, &_edit_context);
               ImGui::SameLine();
               ImGui::Checkbox("Shadow Caster", light, &world::light::shadow_caster,
                               &_edit_stack_world, &_edit_context);
               ImGui::SameLine();
               ImGui::Checkbox("Specular Caster", light, &world::light::specular_caster,
                               &_edit_stack_world, &_edit_context);

               ImGui::EnumSelect(
                  "Light Type", light, &world::light::light_type,
                  &_edit_stack_world, &_edit_context,
                  {enum_select_option{"Directional", world::light_type::directional},
                   enum_select_option{"Point", world::light_type::point},
                   enum_select_option{"Spot", world::light_type::spot},
                   enum_select_option{"Directional Region Box",
                                      world::light_type::directional_region_box},
                   enum_select_option{"Directional Region Sphere",
                                      world::light_type::directional_region_sphere},
                   enum_select_option{"Directional Region Cylinder",
                                      world::light_type::directional_region_cylinder}});

               ImGui::Separator();

               ImGui::DragFloat("Range", light, &world::light::range,
                                &_edit_stack_world, &_edit_context);
               ImGui::DragFloat("Inner Cone Angle", light,
                                &world::light::inner_cone_angle, &_edit_stack_world,
                                &_edit_context, 0.01f, 0.0f, light->outer_cone_angle,
                                "%.3f", ImGuiSliderFlags_AlwaysClamp);
               ImGui::DragFloat("Outer Cone Angle", light,
                                &world::light::outer_cone_angle, &_edit_stack_world,
                                &_edit_context, 0.01f, light->inner_cone_angle,
                                1.570f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

               ImGui::Separator();

               ImGui::DragFloat2("Directional Texture Tiling", light,
                                 &world::light::directional_texture_tiling,
                                 &_edit_stack_world, &_edit_context, 0.01f);
               ImGui::DragFloat2("Directional Texture Offset", light,
                                 &world::light::directional_texture_offset,
                                 &_edit_stack_world, &_edit_context, 0.01f);

               ImGui::InputTextAutoComplete(
                  "Texture", light, &world::light::texture, &_edit_stack_world,
                  &_edit_context, [&] {
                     std::array<std::string, 6> entries;
                     std::size_t matching_count = 0;

                     _asset_libraries.textures.enumerate_known(
                        [&](const lowercase_string& asset) {
                           if (matching_count == entries.size()) return;
                           if (not asset.contains(light->texture)) return;

                           entries[matching_count] = asset;

                           ++matching_count;
                        });

                     return entries;
                  });

               ImGui::Separator();

               ImGui::InputText("Region Name", light, &world::light::region_name,
                                &_edit_stack_world, &_edit_context);
               ImGui::DragQuat("Region Rotation", light, &world::light::region_rotation,
                               &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Region Size", light, &world::light::region_size,
                                 &_edit_stack_world, &_edit_context);
            },
            [&](world::path_id id) {
               world::path* path =
                  look_for(_world.paths, [id](const world::path& path) {
                     return id == path.id;
                  });

               if (not path) return;

               ImGui::InputText("Name", path, &world::path::name,
                                &_edit_stack_world, &_edit_context);
               ImGui::LayerPick("Layer", path, &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::EnumSelect(
                  "Spline Type", path, &world::path::spline_type,
                  &_edit_stack_world, &_edit_context,
                  {enum_select_option{"None", world::path_spline_type::none},
                   enum_select_option{"Linear", world::path_spline_type::linear},
                   enum_select_option{"Hermite", world::path_spline_type::hermite},
                   enum_select_option{"Catmull-Rom",
                                      world::path_spline_type::catmull_rom}});

               for (auto [i, prop] : enumerate(path->properties)) {
                  ImGui::InputKeyValue(path, &world::path::properties, i,
                                       &_edit_stack_world, &_edit_context);
               }

               ImGui::Text("Nodes");
               ImGui::BeginChild("Nodes", {}, true);

               for (auto [i, node] : enumerate(path->nodes)) {
                  ImGui::PushID(static_cast<int>(i));

                  ImGui::Text("Node %i", static_cast<int>(i));
                  ImGui::Separator();

                  ImGui::DragQuat("Rotation", path, i, &world::path::node::rotation,
                                  &_edit_stack_world, &_edit_context);
                  ImGui::DragFloat3("Position", path, i, &world::path::node::position,
                                    &_edit_stack_world, &_edit_context);

                  for (auto [prop_index, prop] : enumerate(node.properties)) {
                     ImGui::InputKeyValue(path, i, prop_index,
                                          &_edit_stack_world, &_edit_context);
                  }

                  ImGui::PopID();
               }

               ImGui::EndChild();
            },
            [&](world::path_id_node_pair id_node) {
               auto [id, node_index] = id_node;

               world::path* path =
                  look_for(_world.paths, [id](const world::path& path) {
                     return id == path.id;
                  });

               if (not path) return;

               if (node_index >= path->nodes.size()) return;
            },
            [&](world::region_id id) {
               world::region* region =
                  look_for(_world.regions, [id](const world::region& region) {
                     return id == region.id;
                  });

               if (not region) return;

               ImGui::InputText("Name", region, &world::region::name,
                                &_edit_stack_world, &_edit_context);
               ImGui::LayerPick("Layer", region, &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::DragQuat("Rotation", region, &world::region::rotation,
                               &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Position", region, &world::region::position,
                                 &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Size", region, &world::region::size, &_edit_stack_world,
                                 &_edit_context, 1.0f, 0.0f, 1e10f);

               ImGui::EnumSelect("Shape", region, &world::region::shape,
                                 &_edit_stack_world, &_edit_context,
                                 {enum_select_option{"Box", world::region_shape::box},
                                  enum_select_option{"Sphere", world::region_shape::sphere},
                                  enum_select_option{"Cylinder",
                                                     world::region_shape::cylinder}});

               ImGui::Separator();

               ImGui::InputText("Description", region, &world::region::description,
                                &_edit_stack_world, &_edit_context);
            },
            [&](world::sector_id id) {
               world::sector* sector =
                  look_for(_world.sectors, [id](const world::sector& sector) {
                     return id == sector.id;
                  });

               if (not sector) return;
            },
            [&](world::portal_id id) {
               world::portal* portal =
                  look_for(_world.portals, [id](const world::portal& portal) {
                     return id == portal.id;
                  });

               if (not portal) return;
            },
            [&](world::hintnode_id id) {
               world::hintnode* hintnode =
                  look_for(_world.hintnodes, [id](const world::hintnode& hintnode) {
                     return id == hintnode.id;
                  });

               if (not hintnode) return;

               ImGui::InputText("Name", hintnode, &world::hintnode::name,
                                &_edit_stack_world, &_edit_context);
               ImGui::LayerPick("Layer", hintnode, &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               ImGui::DragQuat("Rotation", hintnode, &world::hintnode::rotation,
                               &_edit_stack_world, &_edit_context);
               ImGui::DragFloat3("Position", hintnode, &world::hintnode::position,
                                 &_edit_stack_world, &_edit_context);
            },
            [&](world::barrier_id id) {
               world::barrier* barrier =
                  look_for(_world.barriers, [id](const world::barrier& barrier) {
                     return id == barrier.id;
                  });

               if (not barrier) return;
            },
            [&](world::planning_hub_id id) { (void)id; },
            [&](world::planning_connection_id id) { (void)id; },
            [&](world::boundary_id id) {
               world::boundary* boundary =
                  look_for(_world.boundaries, [id](const world::boundary& boundary) {
                     return id == boundary.id;
                  });

               if (not boundary) return;

               world::path* path =
                  look_for(_world.paths, [&](const world::path& path) {
                     return path.name == boundary->name;
                  });

               if (not path) return;
            },
         },
         _interaction_targets.selection.front());

      ImGui::End();

      if (not selection_open) _interaction_targets.selection.clear();
   }

   if (_interaction_targets.creation_entity) {
      if (std::exchange(_entity_creation_context.activate_point_at, false)) {
         _entity_creation_context.placement_rotation =
            placement_rotation::manual_quaternion;
         _entity_creation_context.placement_mode = placement_mode::manual;

         _edit_stack_world.close_last(); // Make sure we don't coalesce with a previous point at.
         _entity_creation_context.using_point_at = true;
      }

      if (std::exchange(_entity_creation_context.activate_extend_to, false)) {
         _edit_stack_world.close_last();

         _entity_creation_context.using_extend_to = true;
         _entity_creation_context.using_shrink_to = false;
      }

      if (std::exchange(_entity_creation_context.activate_shrink_to, false)) {
         _edit_stack_world.close_last();

         _entity_creation_context.using_extend_to = false;
         _entity_creation_context.using_shrink_to = true;
      }

      if (std::exchange(_entity_creation_context.activate_from_object_bbox, false)) {
         _edit_stack_world.close_last();

         _entity_creation_context.using_from_object_bbox = true;
      }

      bool continue_creation = true;

      ImGui::SetNextWindowPos({232.0f * _display_scale, 32.0f * _display_scale},
                              ImGuiCond_Once, {0.0f, 0.0f});

      ImGui::Begin("Create", &continue_creation,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                      ImGuiWindowFlags_AlwaysAutoResize);

      world::creation_entity& creation_entity = *_interaction_targets.creation_entity;

      const placement_traits traits = std::visit(
         overload{
            [&](const world::object& object) {
               ImGui::Text("Object");
               ImGui::Separator();

               ImGui::InputText("Name", &creation_entity, &world::object::name,
                                &_edit_stack_world, &_edit_context,
                                [&](std::string* edited_value) {
                                   *edited_value =
                                      world::create_unique_name(_world.objects,
                                                                *edited_value);
                                });

               ImGui::InputTextAutoComplete(
                  "Class Name", &creation_entity, &world::object::class_name,
                  &_edit_stack_world, &_edit_context, [&] {
                     std::array<std::string, 6> entries;
                     std::size_t matching_count = 0;

                     _asset_libraries.odfs.enumerate_known(
                        [&](const lowercase_string& asset) {
                           if (matching_count == entries.size()) return;
                           if (not asset.contains(object.class_name)) return;

                           entries[matching_count] = asset;

                           ++matching_count;
                        });

                     return entries;
                  });
               ImGui::LayerPick<world::object>("Layer", &creation_entity,
                                               &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               if (_entity_creation_context.placement_rotation !=
                   placement_rotation::manual_quaternion) {
                  ImGui::DragRotationEuler("Rotation", &creation_entity,
                                           &world::object::rotation,
                                           &world::edit_context::euler_rotation,
                                           &_edit_stack_world, &_edit_context);
               }
               else {
                  ImGui::DragQuat("Rotation", &creation_entity, &world::object::rotation,
                                  &_edit_stack_world, &_edit_context);
               }

               if (ImGui::DragFloat3("Position", &creation_entity, &world::object::position,
                                     &_edit_stack_world, &_edit_context)) {
                  _entity_creation_context.placement_mode = placement_mode::manual;
               }

               if ((_entity_creation_context.placement_rotation ==
                       placement_rotation::surface or
                    _entity_creation_context.placement_mode == placement_mode::cursor) and
                   not _entity_creation_context.using_point_at) {
                  quaternion new_rotation = object.rotation;
                  float3 new_position = object.position;
                  float3 new_euler_rotation = _edit_context.euler_rotation;

                  if (_entity_creation_context.placement_rotation ==
                         placement_rotation::surface and
                      _cursor_surface_normalWS) {
                     const float new_y_angle =
                        surface_rotation_degrees(*_cursor_surface_normalWS,
                                                 _edit_context.euler_rotation.y);
                     new_euler_rotation = {_edit_context.euler_rotation.x, new_y_angle,
                                           _edit_context.euler_rotation.z};
                     new_rotation = make_quat_from_euler(
                        new_euler_rotation * std::numbers::pi_v<float> / 180.0f);
                  }

                  if (_entity_creation_context.placement_mode == placement_mode::cursor) {
                     new_position = _cursor_positionWS;

                     if (_entity_creation_context.placement_ground ==
                            placement_ground::bbox and
                         _object_classes.contains(object.class_name)) {

                        const math::bounding_box bbox =
                           object.rotation *
                           _object_classes.at(object.class_name).model->bounding_box;

                        new_position.y -= bbox.min.y;
                     }

                     if (_entity_creation_context.placement_alignment ==
                         placement_alignment::grid) {
                        new_position =
                           align_position_to_grid(new_position,
                                                  _entity_creation_context.alignment);
                     }
                     else if (_entity_creation_context.placement_alignment ==
                              placement_alignment::snapping) {
                        const std::optional<float3> snapped_position =
                           world::get_snapped_position(object, new_position,
                                                       _world.objects,
                                                       _entity_creation_context.snap_distance,
                                                       _object_classes);

                        if (snapped_position) new_position = *snapped_position;
                     }

                     if (_entity_creation_context.lock_x_axis) {
                        new_position.x = object.position.x;
                     }
                     if (_entity_creation_context.lock_y_axis) {
                        new_position.y = object.position.y;
                     }
                     if (_entity_creation_context.lock_z_axis) {
                        new_position.z = object.position.z;
                     }
                  }

                  if (new_rotation != object.rotation or new_position != object.position) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_location<world::object>>(
                           new_rotation, object.rotation, new_position, object.position,
                           new_euler_rotation, _edit_context.euler_rotation),
                        _edit_context);
                  }
               }

               if (_entity_creation_context.using_point_at) {
                  _tool_visualizers.lines.emplace_back(_cursor_positionWS,
                                                       object.position, 0xffffffffu);

                  const quaternion new_rotation =
                     look_at_quat(_cursor_positionWS, object.position);

                  if (new_rotation != object.rotation) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_value<world::object, quaternion>>(
                           &world::object::rotation, new_rotation, object.rotation),
                        _edit_context);
                  }
               }

               ImGui::Separator();

               ImGui::SliderInt("Team", &creation_entity, &world::object::team,
                                &_edit_stack_world, &_edit_context, 0, 15, "%d",
                                ImGuiSliderFlags_AlwaysClamp);

               return placement_traits{};
            },
            [&](const world::light& light) {
               ImGui::InputText("Name", &creation_entity, &world::light::name,
                                &_edit_stack_world, &_edit_context,
                                [&](std::string* edited_value) {
                                   *edited_value =
                                      world::create_unique_name(_world.lights,
                                                                *edited_value);
                                });

               ImGui::LayerPick<world::light>("Layer", &creation_entity,
                                              &_edit_stack_world, &_edit_context);

               ImGui::Separator();

               if (_entity_creation_context.placement_rotation !=
                   placement_rotation::manual_quaternion) {
                  ImGui::DragRotationEuler("Rotation", &creation_entity,
                                           &world::light::rotation,
                                           &world::edit_context::euler_rotation,
                                           &_edit_stack_world, &_edit_context);
               }
               else {
                  ImGui::DragQuat("Rotation", &creation_entity, &world::light::rotation,
                                  &_edit_stack_world, &_edit_context);
               }

               if (ImGui::DragFloat3("Position", &creation_entity, &world::light::position,
                                     &_edit_stack_world, &_edit_context)) {
                  _entity_creation_context.placement_mode = placement_mode::manual;
               }

               if ((_entity_creation_context.placement_rotation ==
                       placement_rotation::surface or
                    _entity_creation_context.placement_mode == placement_mode::cursor) and
                   not _entity_creation_context.using_point_at) {
                  quaternion new_rotation = light.rotation;
                  float3 new_position = light.position;
                  float3 new_euler_rotation = _edit_context.euler_rotation;

                  if (_entity_creation_context.placement_rotation ==
                         placement_rotation::surface and
                      _cursor_surface_normalWS) {
                     const float new_y_angle =
                        surface_rotation_degrees(*_cursor_surface_normalWS,
                                                 _edit_context.euler_rotation.y);
                     new_euler_rotation = {_edit_context.euler_rotation.x, new_y_angle,
                                           _edit_context.euler_rotation.z};
                     new_rotation = make_quat_from_euler(
                        new_euler_rotation * std::numbers::pi_v<float> / 180.0f);
                  }

                  if (_entity_creation_context.placement_mode == placement_mode::cursor) {
                     new_position = _cursor_positionWS;
                     if (_entity_creation_context.placement_alignment ==
                         placement_alignment::grid) {
                        new_position =
                           align_position_to_grid(new_position,
                                                  _entity_creation_context.alignment);
                     }
                     else if (_entity_creation_context.placement_alignment ==
                              placement_alignment::snapping) {
                        const std::optional<float3> snapped_position =
                           world::get_snapped_position(new_position, _world.objects,
                                                       _entity_creation_context.snap_distance,
                                                       _object_classes);

                        if (snapped_position) new_position = *snapped_position;
                     }

                     if (_entity_creation_context.lock_x_axis) {
                        new_position.x = light.position.x;
                     }
                     if (_entity_creation_context.lock_y_axis) {
                        new_position.y = light.position.y;
                     }
                     if (_entity_creation_context.lock_z_axis) {
                        new_position.z = light.position.z;
                     }
                  }

                  if (new_rotation != light.rotation or new_position != light.position) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_location<world::light>>(
                           new_rotation, light.rotation, new_position, light.position,
                           new_euler_rotation, _edit_context.euler_rotation),
                        _edit_context);
                  }
               }

               if (_entity_creation_context.using_point_at) {
                  _tool_visualizers.lines.emplace_back(_cursor_positionWS,
                                                       light.position, 0xffffffffu);

                  const quaternion new_rotation =
                     look_at_quat(_cursor_positionWS, light.position);

                  if (new_rotation != light.rotation) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_value<world::light, quaternion>>(
                           &world::light::rotation, new_rotation, light.rotation),
                        _edit_context);
                  }
               }

               ImGui::Separator();

               ImGui::ColorEdit3("Color", &creation_entity, &world::light::color,
                                 &_edit_stack_world, &_edit_context,
                                 ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

               ImGui::Checkbox("Static", &creation_entity, &world::light::static_,
                               &_edit_stack_world, &_edit_context);
               ImGui::SameLine();
               ImGui::Checkbox("Shadow Caster", &creation_entity,
                               &world::light::shadow_caster, &_edit_stack_world,
                               &_edit_context);
               ImGui::SameLine();
               ImGui::Checkbox("Specular Caster", &creation_entity,
                               &world::light::specular_caster,
                               &_edit_stack_world, &_edit_context);

               ImGui::EnumSelect(
                  "Light Type", &creation_entity, &world::light::light_type,
                  &_edit_stack_world, &_edit_context,
                  {enum_select_option{"Directional", world::light_type::directional},
                   enum_select_option{"Point", world::light_type::point},
                   enum_select_option{"Spot", world::light_type::spot},
                   enum_select_option{"Directional Region Box",
                                      world::light_type::directional_region_box},
                   enum_select_option{"Directional Region Sphere",
                                      world::light_type::directional_region_sphere},
                   enum_select_option{"Directional Region Cylinder",
                                      world::light_type::directional_region_cylinder}});

               ImGui::Separator();

               if (light.light_type == world::light_type::point or
                   light.light_type == world::light_type::spot) {
                  ImGui::DragFloat("Range", &creation_entity, &world::light::range,
                                   &_edit_stack_world, &_edit_context);

                  if (light.light_type == world::light_type::spot) {
                     ImGui::DragFloat("Inner Cone Angle", &creation_entity,
                                      &world::light::inner_cone_angle,
                                      &_edit_stack_world, &_edit_context, 0.01f,
                                      0.0f, light.outer_cone_angle, "%.3f",
                                      ImGuiSliderFlags_AlwaysClamp);
                     ImGui::DragFloat("Outer Cone Angle", &creation_entity,
                                      &world::light::outer_cone_angle,
                                      &_edit_stack_world, &_edit_context, 0.01f,
                                      light.inner_cone_angle, 1.570f, "%.3f",
                                      ImGuiSliderFlags_AlwaysClamp);
                  }

                  ImGui::Separator();
               }

               ImGui::InputTextAutoComplete(
                  "Texture", &creation_entity, &world::light::texture,
                  &_edit_stack_world, &_edit_context, [&] {
                     std::array<std::string, 6> entries;
                     std::size_t matching_count = 0;

                     _asset_libraries.textures.enumerate_known(
                        [&](const lowercase_string& asset) {
                           if (matching_count == entries.size()) return;
                           if (not asset.contains(light.texture)) return;

                           entries[matching_count] = asset;

                           ++matching_count;
                        });

                     return entries;
                  });

               if (world::is_directional_light(light) and not light.texture.empty()) {
                  ImGui::DragFloat2("Directional Texture Tiling", &creation_entity,
                                    &world::light::directional_texture_tiling,
                                    &_edit_stack_world, &_edit_context, 0.01f);
                  ImGui::DragFloat2("Directional Texture Offset", &creation_entity,
                                    &world::light::directional_texture_offset,
                                    &_edit_stack_world, &_edit_context, 0.01f);
               }

               if (is_region_light(light)) {
                  ImGui::Separator();

                  ImGui::InputText("Region Name", &creation_entity,
                                   &world::light::region_name, &_edit_stack_world,
                                   &_edit_context, [&](std::string* edited_value) {
                                      *edited_value =
                                         world::create_unique_light_region_name(
                                            _world.lights, _world.regions,
                                            light.region_name.empty()
                                               ? light.name
                                               : light.region_name);
                                   });

                  if (_entity_creation_context.placement_rotation !=
                      placement_rotation::manual_quaternion) {
                     ImGui::DragRotationEuler("Rotation", &creation_entity,
                                              &world::light::region_rotation,
                                              &world::edit_context::light_region_euler_rotation,
                                              &_edit_stack_world, &_edit_context);
                  }
                  else {
                     ImGui::DragQuat("Region Rotation", &creation_entity,
                                     &world::light::region_rotation,
                                     &_edit_stack_world, &_edit_context);
                  }

                  ImGui::DragFloat3("Region Size", &creation_entity,
                                    &world::light::region_size,
                                    &_edit_stack_world, &_edit_context);
               }

               return placement_traits{.has_placement_ground = false};
            },
            [&](const world::path& path) {
               ImGui::InputText("Name", &creation_entity, &world::path::name,
                                &_edit_stack_world, &_edit_context,
                                [&](std::string* edited_value) {
                                   *edited_value =
                                      world::create_unique_name(_world.paths,
                                                                path.name.empty()
                                                                   ? "Path 0"sv
                                                                   : path.name);
                                });

               ImGui::LayerPick<world::path>("Layer", &creation_entity,
                                             &_edit_stack_world, &_edit_context);

               ImGui::EnumSelect(
                  "Spline Type", &creation_entity, &world::path::spline_type,
                  &_edit_stack_world, &_edit_context,
                  {enum_select_option{"None", world::path_spline_type::none},
                   enum_select_option{"Linear", world::path_spline_type::linear},
                   enum_select_option{"Hermite", world::path_spline_type::hermite},
                   enum_select_option{"Catmull-Rom",
                                      world::path_spline_type::catmull_rom}});

               ImGui::Separator();

               if (path.nodes.size() != 1) std::terminate();

               ImGui::Text("Next Node");

               if (_entity_creation_context.placement_rotation !=
                   placement_rotation::manual_quaternion) {
                  ImGui::DragRotationEulerPathNode("Rotation", &creation_entity,
                                                   &world::edit_context::euler_rotation,
                                                   &_edit_stack_world, &_edit_context);
               }
               else {
                  ImGui::DragQuatPathNode("Rotation", &creation_entity,
                                          &_edit_stack_world, &_edit_context);
               }

               if (ImGui::DragFloat3PathNode("Position", &creation_entity,
                                             &_edit_stack_world, &_edit_context)) {
                  _entity_creation_context.placement_mode = placement_mode::manual;
               }
               if ((_entity_creation_context.placement_rotation ==
                       placement_rotation::surface or
                    _entity_creation_context.placement_mode == placement_mode::cursor) and
                   not _entity_creation_context.using_point_at) {
                  quaternion new_rotation = path.nodes[0].rotation;
                  float3 new_position = path.nodes[0].position;
                  float3 new_euler_rotation = _edit_context.euler_rotation;

                  if (_entity_creation_context.placement_rotation ==
                         placement_rotation::surface and
                      _cursor_surface_normalWS) {
                     const float new_y_angle =
                        surface_rotation_degrees(*_cursor_surface_normalWS,
                                                 _edit_context.euler_rotation.y);
                     new_euler_rotation = {_edit_context.euler_rotation.x, new_y_angle,
                                           _edit_context.euler_rotation.z};
                     new_rotation = make_quat_from_euler(
                        new_euler_rotation * std::numbers::pi_v<float> / 180.0f);
                  }

                  if (_entity_creation_context.placement_mode == placement_mode::cursor) {
                     new_position = _cursor_positionWS;

                     if (_entity_creation_context.placement_alignment ==
                         placement_alignment::grid) {
                        new_position =
                           align_position_to_grid(new_position,
                                                  _entity_creation_context.alignment);
                     }
                     else if (_entity_creation_context.placement_alignment ==
                              placement_alignment::snapping) {
                        const std::optional<float3> snapped_position =
                           world::get_snapped_position(new_position, _world.objects,
                                                       _entity_creation_context.snap_distance,
                                                       _object_classes);

                        if (snapped_position) new_position = *snapped_position;
                     }

                     if (_entity_creation_context.lock_x_axis) {
                        new_position.x = path.nodes[0].position.x;
                     }
                     if (_entity_creation_context.lock_y_axis) {
                        new_position.y = path.nodes[0].position.y;
                     }
                     if (_entity_creation_context.lock_z_axis) {
                        new_position.z = path.nodes[0].position.z;
                     }
                  }

                  if (new_rotation != path.nodes[0].rotation or
                      new_position != path.nodes[0].position) {
                     _edit_stack_world.apply(std::make_unique<edits::set_creation_path_node_location>(
                                                new_rotation, path.nodes[0].rotation,
                                                new_position, path.nodes[0].position,
                                                new_euler_rotation,
                                                _edit_context.euler_rotation),
                                             _edit_context);
                  }
               }

               if (_entity_creation_context.using_point_at) {
                  _tool_visualizers.lines.emplace_back(_cursor_positionWS,
                                                       path.nodes[0].position,
                                                       0xffffffffu);

                  const quaternion new_rotation =
                     look_at_quat(_cursor_positionWS, path.nodes[0].position);

                  if (new_rotation != path.nodes[0].rotation) {
                     _edit_stack_world
                        .apply(std::make_unique<edits::set_creation_path_node_value<quaternion>>(
                                  &world::path::node::rotation, new_rotation,
                                  path.nodes[0].rotation),
                               _edit_context);
                  }
               }

               ImGui::Separator();

               if (ImGui::Button("New Path", {ImGui::CalcItemWidth(), 0.0f}) or
                   std::exchange(_entity_creation_context.finish_current_path, false)) {

                  _edit_stack_world.apply(
                     std::make_unique<edits::set_creation_value<world::path, std::string>>(
                        &world::path::name,
                        world::create_unique_name(_world.paths, path.name), path.name),
                     _edit_context);
               }

               if (ImGui::IsItemHovered()) {
                  ImGui::SetTooltip("Create another new path and stop adding "
                                    "nodes to the current one.");
               }

               return placement_traits{.has_new_path = true,
                                       .has_node_placement_insert = true};
            },
            [&](const world::region& region) {
               ImGui::InputText("Name", &creation_entity, &world::region::name,
                                &_edit_stack_world, &_edit_context,
                                [&](std::string* edited_value) {
                                   *edited_value =
                                      world::create_unique_name(_world.regions,
                                                                region.name);
                                });

               ImGui::LayerPick<world::region>("Layer", &creation_entity,
                                               &_edit_stack_world, &_edit_context);

               ImGui::InputText("Description", &creation_entity,
                                &world::region::description, &_edit_stack_world,
                                &_edit_context,
                                [&]([[maybe_unused]] std::string* edited_value) {});

               ImGui::Separator();

               if (_entity_creation_context.placement_rotation !=
                   placement_rotation::manual_quaternion) {
                  ImGui::DragRotationEuler("Rotation", &creation_entity,
                                           &world::region::rotation,
                                           &world::edit_context::euler_rotation,
                                           &_edit_stack_world, &_edit_context);
               }
               else {
                  ImGui::DragQuat("Rotation", &creation_entity, &world::region::rotation,
                                  &_edit_stack_world, &_edit_context);
               }

               if (ImGui::DragFloat3("Position", &creation_entity, &world::region::position,
                                     &_edit_stack_world, &_edit_context)) {
                  _entity_creation_context.placement_mode = placement_mode::manual;
               }

               if ((_entity_creation_context.placement_rotation ==
                       placement_rotation::surface or
                    _entity_creation_context.placement_mode == placement_mode::cursor) and
                   not _entity_creation_context.using_point_at) {
                  quaternion new_rotation = region.rotation;
                  float3 new_position = region.position;
                  float3 new_euler_rotation = _edit_context.euler_rotation;

                  if (_entity_creation_context.placement_rotation ==
                         placement_rotation::surface and
                      _cursor_surface_normalWS) {
                     const float new_y_angle =
                        surface_rotation_degrees(*_cursor_surface_normalWS,
                                                 _edit_context.euler_rotation.y);
                     new_euler_rotation = {_edit_context.euler_rotation.x, new_y_angle,
                                           _edit_context.euler_rotation.z};
                     new_rotation = make_quat_from_euler(
                        new_euler_rotation * std::numbers::pi_v<float> / 180.0f);
                  }

                  if (_entity_creation_context.placement_mode == placement_mode::cursor) {
                     new_position = _cursor_positionWS;

                     if (_entity_creation_context.placement_ground ==
                         placement_ground::bbox) {
                        switch (region.shape) {
                        case world::region_shape::box: {
                           const std::array<float3, 8> bbox_corners = math::to_corners(
                              {.min = -region.size, .max = region.size});

                           float min_y = std::numeric_limits<float>::max();
                           float max_y = std::numeric_limits<float>::lowest();

                           for (const float3& v : bbox_corners) {
                              const float3 rotated_corner = region.rotation * v;

                              min_y = std::min(rotated_corner.y, min_y);
                              max_y = std::max(rotated_corner.y, max_y);
                           }

                           new_position.y += (std::abs(max_y - min_y) / 2.0f);
                        } break;
                        case world::region_shape::sphere: {
                           new_position.y += length(region.size);
                        } break;
                        case world::region_shape::cylinder: {
                           const float cylinder_radius =
                              length(float2{region.size.x, region.size.z});
                           const std::array<float3, 8> bbox_corners = math::to_corners(
                              {.min = {-cylinder_radius, -region.size.y, -cylinder_radius},
                               .max = {cylinder_radius, region.size.y, cylinder_radius}});

                           float min_y = std::numeric_limits<float>::max();
                           float max_y = std::numeric_limits<float>::lowest();

                           for (const float3& v : bbox_corners) {
                              const float3 rotated_corner = region.rotation * v;

                              min_y = std::min(rotated_corner.y, min_y);
                              max_y = std::max(rotated_corner.y, max_y);
                           }

                           new_position.y += (std::abs(max_y - min_y) / 2.0f);
                        } break;
                        }
                     }

                     if (_entity_creation_context.placement_alignment ==
                         placement_alignment::grid) {
                        new_position =
                           align_position_to_grid(new_position,
                                                  _entity_creation_context.alignment);
                     }
                     else if (_entity_creation_context.placement_alignment ==
                              placement_alignment::snapping) {
                        const std::optional<float3> snapped_position =
                           world::get_snapped_position(new_position, _world.objects,
                                                       _entity_creation_context.snap_distance,
                                                       _object_classes);

                        if (snapped_position) new_position = *snapped_position;
                     }

                     if (_entity_creation_context.lock_x_axis) {
                        new_position.x = region.position.x;
                     }
                     if (_entity_creation_context.lock_y_axis) {
                        new_position.y = region.position.y;
                     }
                     if (_entity_creation_context.lock_z_axis) {
                        new_position.z = region.position.z;
                     }
                  }

                  if (new_rotation != region.rotation or new_position != region.position) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_location<world::region>>(
                           new_rotation, region.rotation, new_position, region.position,
                           new_euler_rotation, _edit_context.euler_rotation),
                        _edit_context);
                  }
               }

               if (_entity_creation_context.using_point_at) {
                  _tool_visualizers.lines.emplace_back(_cursor_positionWS,
                                                       region.position, 0xffffffffu);

                  const quaternion new_rotation =
                     look_at_quat(_cursor_positionWS, region.position);

                  if (new_rotation != region.rotation) {
                     _edit_stack_world.apply(
                        std::make_unique<edits::set_creation_value<world::region, quaternion>>(
                           &world::region::rotation, new_rotation, region.rotation),
                        _edit_context);
                  }
               }

               ImGui::Separator();
               ImGui::EnumSelect("Shape", &creation_entity, &world::region::shape,
                                 &_edit_stack_world, &_edit_context,
                                 {enum_select_option{"Box", world::region_shape::box},
                                  enum_select_option{"Sphere", world::region_shape::sphere},
                                  enum_select_option{"Cylinder",
                                                     world::region_shape::cylinder}});

               float3 region_size = region.size;

               switch (region.shape) {
               case world::region_shape::box: {
                  ImGui::DragFloat3("Size", &region_size, 0.0f, 1e10f);
               } break;
               case world::region_shape::sphere: {
                  float radius = length(region_size);

                  if (ImGui::DragFloat("Radius", &radius, 0.1f)) {
                     const float radius_sq = radius * radius;
                     const float size = std::sqrt(radius_sq / 3.0f);

                     region_size = {size, size, size};
                  }
               } break;
               case world::region_shape::cylinder: {
                  float height = region_size.y * 2.0f;

                  if (ImGui::DragFloat("Height", &height, 0.1f, 0.0f, 1e10f)) {
                     region_size.y = height / 2.0f;
                  }

                  float radius = length(float2{region_size.x, region_size.z});

                  if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 1e10f)) {
                     const float radius_sq = radius * radius;
                     const float size = std::sqrt(radius_sq / 2.0f);

                     region_size.x = size;
                     region_size.z = size;
                  }
               } break;
               }

               if (ImGui::Button("Extend To", {ImGui::CalcItemWidth(), 0.0f})) {
                  _entity_creation_context.activate_extend_to = true;
               }
               if (ImGui::Button("Shrink To", {ImGui::CalcItemWidth(), 0.0f})) {
                  _entity_creation_context.activate_shrink_to = true;

                  _entity_creation_context.using_shrink_to =
                     not _entity_creation_context.using_shrink_to;
                  _entity_creation_context.using_extend_to = false;
               }

               if (_entity_creation_context.using_extend_to or
                   _entity_creation_context.using_shrink_to) {
                  _entity_creation_context.placement_mode = placement_mode::manual;

                  if (not _entity_creation_context.resize_start_size) {
                     _entity_creation_context.resize_start_size = region.size;
                  }

                  _tool_visualizers.lines.emplace_back(_cursor_positionWS,
                                                       region.position, 0xffffffffu);

                  const float3 region_start_size =
                     *_entity_creation_context.resize_start_size;

                  switch (region.shape) {
                  case world::region_shape::box: {
                     const quaternion inverse_region_rotation =
                        conjugate(region.rotation);

                     const float3 cursorRS = inverse_region_rotation *
                                             (_cursor_positionWS - region.position);

                     if (_entity_creation_context.using_extend_to) {
                        region_size = max(abs(cursorRS), region_start_size);
                     }
                     else {
                        region_size = min(abs(cursorRS), region_start_size);
                     }
                  } break;
                  case world::region_shape::sphere: {
                     const float start_radius = length(region_start_size);
                     const float new_radius =
                        distance(region.position, _cursor_positionWS);

                     const float radius = _entity_creation_context.using_extend_to
                                             ? std::max(start_radius, new_radius)
                                             : std::min(start_radius, new_radius);
                     const float radius_sq = radius * radius;
                     const float size = std::sqrt(radius_sq / 3.0f);

                     region_size = {size, size, size};
                  } break;
                  case world::region_shape::cylinder: {
                     const float start_radius =
                        length(float2{region_start_size.x, region_start_size.z});
                     const float start_height = region_start_size.y;

                     const quaternion inverse_region_rotation =
                        conjugate(region.rotation);

                     const float3 cursorRS = inverse_region_rotation *
                                             (_cursor_positionWS - region.position);

                     const float new_radius = length(float2{cursorRS.x, cursorRS.z});
                     const float new_height = std::abs(cursorRS.y);

                     const float radius = std::max(start_radius, new_radius);
                     const float radius_sq = radius * radius;
                     const float size = std::sqrt(radius_sq / 2.0f);

                     region_size = {size, std::max(start_height, new_height), size};
                  }
                  }
               }
               else {
                  _entity_creation_context.resize_start_size = std::nullopt;
               }

               if (region_size != region.size) {
                  _edit_stack_world.apply(
                     std::make_unique<edits::set_creation_value<world::region, float3>>(
                        &world::region::size, region_size, region.size),
                     _edit_context);
               }

               ImGui::Separator();
               if (ImGui::Button("From Object Bounds", {ImGui::CalcItemWidth(), 0.0f})) {
                  _entity_creation_context.activate_from_object_bbox = true;
               }

               if (_entity_creation_context.using_from_object_bbox and
                   _interaction_targets.hovered_entity and
                   std::holds_alternative<world::object_id>(
                      *_interaction_targets.hovered_entity)) {
                  _entity_creation_context.placement_rotation =
                     placement_rotation::manual_quaternion;
                  _entity_creation_context.placement_mode = placement_mode::manual;

                  const world::object* object =
                     world::find_entity(_world.objects,
                                        std::get<world::object_id>(
                                           *_interaction_targets.hovered_entity));

                  if (object) {
                     math::bounding_box bbox =
                        _object_classes.at(object->class_name).model->bounding_box;

                     const float3 size = abs(bbox.max - bbox.min) / 2.0f;
                     const float3 position =
                        object->rotation *
                        ((conjugate(object->rotation) * object->position) +
                         ((bbox.min + bbox.max) / 2.0f));

                     _edit_stack_world.apply(std::make_unique<edits::set_creation_region_metrics>(
                                                object->rotation, region.rotation,
                                                position, region.position, size,
                                                region.size),
                                             _edit_context);
                  }
               }

               return placement_traits{.has_resize_to = true, .has_from_bbox = true};
            },
            [&](const world::sector& sector) {
               ImGui::InputText("Name", &creation_entity, &world::sector::name,
                                &_edit_stack_world, &_edit_context,
                                [&](std::string* edited_value) {
                                   *edited_value =
                                      world::create_unique_name(_world.sectors,
                                                                sector.name);
                                });

               ImGui::DragFloat("Base", &creation_entity, &world::sector::base,
                                &_edit_stack_world, &_edit_context, 1.0f, 0.0f,
                                0.0f, "Y:%.3f");
               ImGui::DragFloat("Height", &creation_entity, &world::sector::height,
                                &_edit_stack_world, &_edit_context);

               if (sector.points.empty()) std::terminate();

               ImGui::DragSectorPoint("Position", &creation_entity,
                                      &_edit_stack_world, &_edit_context);

               if (_entity_creation_context.placement_mode == placement_mode::cursor) {
                  float2 new_position = sector.points[0];

                  new_position = {_cursor_positionWS.x, _cursor_positionWS.z};

                  if (_entity_creation_context.placement_alignment ==
                      placement_alignment::grid) {
                     new_position =
                        align_position_to_grid(new_position,
                                               _entity_creation_context.alignment);
                  }
                  else if (_entity_creation_context.placement_alignment ==
                           placement_alignment::snapping) {
                     // What should snapping for sectors do?
                     ImGui::Text("Snapping is currently unimplemented for "
                                 "sectors. Sorry!");
                  }

                  if (_entity_creation_context.lock_x_axis) {
                     new_position.x = sector.points[0].x;
                  }
                  if (_entity_creation_context.lock_z_axis) {
                     new_position.y = sector.points[0].y;
                  }

                  if (new_position != sector.points[0]) {
                     _edit_stack_world.apply(std::make_unique<edits::set_creation_sector_point>(
                                                new_position, sector.points[0]),
                                             _edit_context);
                  }
               }

               if (ImGui::Button("New Sector", {ImGui::CalcItemWidth(), 0.0f}) or
                   std::exchange(_entity_creation_context.finish_current_sector, false)) {

                  _edit_stack_world.apply(
                     std::make_unique<edits::set_creation_value<world::sector, std::string>>(
                        &world::sector::name,
                        world::create_unique_name(_world.sectors, sector.name),
                        sector.name),
                     _edit_context);
               }

               if (ImGui::IsItemHovered()) {
                  ImGui::SetTooltip("Create another new sector and stop adding "
                                    "points to the current one.");
               }

               ImGui::Checkbox("Auto-Fill Object List",
                               &_entity_creation_context.auto_fill_sector);

               if (ImGui::IsItemHovered()) {
                  ImGui::SetTooltip(
                     "Auto-Fill the sector's object list with objects inside "
                     "the sector from active layers as points are added.");
               }

               if (_entity_creation_context.auto_fill_sector) {
                  ImGui::Text(
                     "Auto-Fill Object List is unimplemented currently.");
               }

               if (const world::sector* existing_sector =
                      world::find_entity(_world.sectors, sector.name);
                   existing_sector and not existing_sector->points.empty()) {
                  const float2 start_point = existing_sector->points.back();
                  const float2 mid_point = sector.points[0];
                  const float2 end_point = existing_sector->points[0];

                  const float3 line_bottom_start = {start_point.x,
                                                    existing_sector->base,
                                                    start_point.y};
                  const float3 line_bottom_mid = {mid_point.x, sector.base,
                                                  mid_point.y};
                  const float3 line_bottom_end = {end_point.x, existing_sector->base,
                                                  end_point.y};

                  const float3 line_top_start = {start_point.x,
                                                 existing_sector->base +
                                                    existing_sector->height,
                                                 start_point.y};
                  const float3 line_top_mid = {mid_point.x, sector.base + sector.height,
                                               mid_point.y};
                  const float3 line_top_end = {end_point.x,
                                               existing_sector->base +
                                                  existing_sector->height,
                                               end_point.y};

                  _tool_visualizers.lines.emplace_back(line_bottom_start,
                                                       line_bottom_mid, 0xffffffffu);
                  _tool_visualizers.lines.emplace_back(line_top_start,
                                                       line_top_mid, 0xffffffffu);
                  _tool_visualizers.lines.emplace_back(line_bottom_mid,
                                                       line_bottom_end, 0xffffffffu);
                  _tool_visualizers.lines.emplace_back(line_top_mid,
                                                       line_top_end, 0xffffffffu);
               }

               return placement_traits{.has_placement_rotation = false,
                                       .has_point_at = false,
                                       .has_placement_ground = false};
            },
            [&](const world::portal& portal) {
               (void)portal;

               return placement_traits{};
            },
            [&](const world::barrier& barrier) {
               (void)barrier;

               return placement_traits{};
            },
            [&](const world::planning_hub& planning_hub) {
               (void)planning_hub;

               return placement_traits{};
            },
            [&](const world::planning_connection& planning_connection) {
               (void)planning_connection;

               return placement_traits{};
            },
            [&](const world::boundary& boundary) {
               (void)boundary;

               return placement_traits{};
            },
         },
         creation_entity);

      if (traits.has_placement_rotation) {
         ImGui::Separator();

         ImGui::Text("Rotation");

         ImGui::BeginTable("Rotation", 3,
                           ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_SizingStretchSame);

         ImGui::TableNextColumn();
         if (ImGui::Selectable("Manual", _entity_creation_context.placement_rotation ==
                                            placement_rotation::manual_euler)) {
            _entity_creation_context.placement_rotation =
               placement_rotation::manual_euler;
         }

         ImGui::TableNextColumn();
         if (ImGui::Selectable("Manual (Quat)",
                               _entity_creation_context.placement_rotation ==
                                  placement_rotation::manual_quaternion)) {
            _entity_creation_context.placement_rotation =
               placement_rotation::manual_quaternion;
         }

         ImGui::TableNextColumn();

         if (ImGui::Selectable("Around Cursor", _entity_creation_context.placement_rotation ==
                                                   placement_rotation::surface)) {
            _entity_creation_context.placement_rotation = placement_rotation::surface;
         }
         ImGui::EndTable();

         if (traits.has_point_at) {
            if (ImGui::Selectable("Point At", _entity_creation_context.using_point_at)) {
               _entity_creation_context.activate_point_at = true;
            }
         }
      }

      if (traits.has_placement_mode) {
         ImGui::Separator();

         ImGui::Text("Placement");

         ImGui::BeginTable("Placement", 2,
                           ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_SizingStretchSame);

         ImGui::TableNextColumn();
         if (ImGui::Selectable("Manual", _entity_creation_context.placement_mode ==
                                            placement_mode::manual)) {
            _entity_creation_context.placement_mode = placement_mode::manual;
         }

         ImGui::TableNextColumn();

         if (ImGui::Selectable("At Cursor", _entity_creation_context.placement_mode ==
                                               placement_mode::cursor)) {
            _entity_creation_context.placement_mode = placement_mode::cursor;
         }
         ImGui::EndTable();
      }

      if (_entity_creation_context.placement_mode == placement_mode::cursor) {
         if (traits.has_lock_axis) {
            ImGui::Separator();

            ImGui::Text("Locked Position");

            ImGui::BeginTable("Locked Position", 3,
                              ImGuiTableFlags_NoSavedSettings |
                                 ImGuiTableFlags_SizingStretchSame);

            ImGui::TableNextColumn();
            ImGui::Selectable("X", &_entity_creation_context.lock_x_axis);
            ImGui::TableNextColumn();
            ImGui::Selectable("Y", &_entity_creation_context.lock_y_axis);
            ImGui::TableNextColumn();
            ImGui::Selectable("Z", &_entity_creation_context.lock_z_axis);

            ImGui::EndTable();
         }

         if (traits.has_placement_alignment) {
            ImGui::Separator();

            ImGui::Text("Align To");

            ImGui::BeginTable("Align To", 3,
                              ImGuiTableFlags_NoSavedSettings |
                                 ImGuiTableFlags_SizingStretchSame);

            ImGui::TableNextColumn();
            if (ImGui::Selectable("None", _entity_creation_context.placement_alignment ==
                                             placement_alignment::none)) {
               _entity_creation_context.placement_alignment =
                  placement_alignment::none;
            }

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Grid", _entity_creation_context.placement_alignment ==
                                             placement_alignment::grid)) {
               _entity_creation_context.placement_alignment =
                  placement_alignment::grid;
            }

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Snapping", _entity_creation_context.placement_alignment ==
                                                 placement_alignment::snapping)) {
               _entity_creation_context.placement_alignment =
                  placement_alignment::snapping;
            }
            ImGui::EndTable();
         }

         if (traits.has_placement_ground) {
            ImGui::Separator();

            ImGui::Text("Ground With");

            ImGui::BeginTable("Ground With", 2,
                              ImGuiTableFlags_NoSavedSettings |
                                 ImGuiTableFlags_SizingStretchSame);

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Origin", _entity_creation_context.placement_ground ==
                                               placement_ground::origin)) {
               _entity_creation_context.placement_ground = placement_ground::origin;
            }

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Bounding Box", _entity_creation_context.placement_ground ==
                                                     placement_ground::bbox)) {
               _entity_creation_context.placement_ground = placement_ground::bbox;
            }

            ImGui::EndTable();
         }

         if (traits.has_node_placement_insert) {
            ImGui::Separator();

            ImGui::Text("Node Insertion");

            ImGui::BeginTable("Node Insertion", 2,
                              ImGuiTableFlags_NoSavedSettings |
                                 ImGuiTableFlags_SizingStretchSame);

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Nearest", _entity_creation_context.placement_node_insert ==
                                                placement_node_insert::nearest)) {
               _entity_creation_context.placement_node_insert =
                  placement_node_insert::nearest;
            }

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Append", _entity_creation_context.placement_node_insert ==
                                               placement_node_insert::append)) {
               _entity_creation_context.placement_node_insert =
                  placement_node_insert::append;
            }

            ImGui::EndTable();
         }

         if (_entity_creation_context.placement_alignment == placement_alignment::grid) {
            ImGui::Separator();
            ImGui::DragFloat("Alignment Grid Size",
                             &_entity_creation_context.alignment, 1.0f, 1.0f,
                             1e10f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
         }
         else if (_entity_creation_context.placement_alignment ==
                  placement_alignment::snapping) {
            ImGui::Separator();
            ImGui::DragFloat("Snap Distance",
                             &_entity_creation_context.snap_distance, 0.1f,
                             0.0f, 1e10f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
         }
      }

      ImGui::End();

      if (_hotkeys_show) {
         ImGui::Begin("Hotkeys");

         if (traits.has_new_path) {
            ImGui::Text("New Path");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.finish_path")));
         }

         if (traits.has_placement_rotation) {
            ImGui::Text("Change Rotation Mode");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.cycle_rotation_mode")));
         }

         if (traits.has_point_at) {
            ImGui::Text("Point At");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.activate_point_at")));
         }

         if (traits.has_placement_mode) {
            ImGui::Text("Change Placement Mode");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.cycle_placement_mode")));
         }

         if (traits.has_lock_axis) {
            ImGui::Text("Lock X Position");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.lock_x_axis")));

            ImGui::Text("Lock Y Position");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.lock_y_axis")));

            ImGui::Text("Lock Z Position");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.lock_z_axis")));
         }

         if (traits.has_placement_alignment) {
            ImGui::Text("Change Alignment Mode");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.cycle_alignment_mode")));
         }

         if (traits.has_placement_ground) {
            ImGui::Text("Change Grounding Mode");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.cycle_ground_mode")));
         }

         if (traits.has_resize_to) {
            ImGui::Text("Extend To");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.activate_extend_to")));

            ImGui::Text("Shrink To");
            ImGui::BulletText(get_display_string(
               _hotkeys.query_binding("Entity Creation",
                                      "entity_creation.activate_shrink_to")));
         }

         if (traits.has_from_bbox) {
            ImGui::Text("From Object Bounds");
            ImGui::BulletText(get_display_string(_hotkeys.query_binding(
               "Entity Creation",
               "entity_creation.activate_from_object_bbox")));
         }

         ImGui::End();
      }

      if (not continue_creation) {
         _entity_creation_context.using_point_at = false;
         _entity_creation_context.using_extend_to = false;
         _entity_creation_context.using_shrink_to = false;
         _entity_creation_context.using_from_object_bbox = false;

         _edit_stack_world.apply(edits::make_creation_entity_set(std::nullopt,
                                                                 creation_entity),
                                 _edit_context);
      }
   }
}
}