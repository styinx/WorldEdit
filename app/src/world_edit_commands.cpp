
#include "edits/creation_entity_set.hpp"
#include "math/vector_funcs.hpp"
#include "world_edit.hpp"

using namespace std::literals;

namespace we {

void world_edit::initialize_commands() noexcept
{
   _commands.add("camera.move_forward"s, _move_camera_forward);
   _commands.add("camera.move_back"s, _move_camera_back);
   _commands.add("camera.move_left"s, _move_camera_left);
   _commands.add("camera.move_right"s, _move_camera_right);
   _commands.add("camera.move_up"s, _move_camera_up);
   _commands.add("camera.move_down"s, _move_camera_down);
   _commands.add("camera.move_sprint_forward"s, [this] {
      _move_camera_forward = not _move_camera_forward;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.move_sprint_back"s, [this] {
      _move_camera_back = not _move_camera_back;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.move_sprint_left"s, [this] {
      _move_camera_left = not _move_camera_left;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.move_sprint_right"s, [this] {
      _move_camera_right = not _move_camera_right;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.move_sprint_up"s, [this] {
      _move_camera_up = not _move_camera_up;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.move_sprint_down"s, [this] {
      _move_camera_down = not _move_camera_down;
      _move_sprint = not _move_sprint;
   });
   _commands.add("camera.rotate_with_mouse"s, [this]() {
      _rotate_camera = not _rotate_camera;
      GetCursorPos(&_rotate_camera_cursor_position);
   });
   _commands.add("camera.pan_with_mouse"s, [this]() {
      _pan_camera = not _pan_camera;
      GetCursorPos(&_rotate_camera_cursor_position);
   });
   _commands.add("camera.zoom_in"s,
                 [this] { _camera.zoom(_camera.zoom() + 0.25f); });
   _commands.add("camera.zoom_out"s,
                 [this] { _camera.zoom(std::max(_camera.zoom() - 0.25f, 1.0f)); });
   _commands.add("camera.step_forward"s, [this] {
      _camera.position(_camera.position() + _camera.forward() * _settings.camera.step_size);
   });
   _commands.add("camera.step_back"s, [this] {
      _camera.position(_camera.position() + _camera.back() * _settings.camera.step_size);
   });
   _commands.add("camera.set_perspective"s, [this]() {
      _camera.projection(graphics::camera_projection::perspective);
   });
   _commands.add("camera.set_orthographic"s, [this]() {
      _camera.projection(graphics::camera_projection::orthographic);
   });
   _commands.add("camera.double_move_speed"s,
                 [this] { _settings.camera.move_speed *= 2.0f; });
   _commands.add("camera.halve_move_speed"s,
                 [this] { _settings.camera.move_speed /= 2.0f; });

   _commands.add("selection.set"s,
                 [this]() { select_hovered_entity(select_method::single); });
   _commands.add("selection.add"s,
                 [this]() { select_hovered_entity(select_method::multi); });
   _commands.add("selection.remove"s, [this]() { deselect_hovered_entity(); });
   _commands.add("selection.clear"s,
                 [this]() { _interaction_targets.selection.clear(); });

   _commands.add("edit.undo"s, [this]() { undo(); });
   _commands.add("edit.redo"s, [this]() { redo(); });
   _commands.add("edit.delete"s, [this]() { delete_selected(); });

   _commands.add("show.hotkeys"s, _hotkeys_view_show);
   _commands.add("show.camera_controls"s, _camera_controls_open);
   _commands.add("show.world_global_lights_editor"s, _world_global_lights_editor_open);
   _commands.add("show.world_layers_editor"s, _world_layers_editor_open);
   _commands.add("show.world_game_mode_editor"s, _world_game_mode_editor_open);
   _commands.add("show.world_requirements_editor"s, _world_requirements_editor_open);
   _commands.add("show.world_explorer"s, _world_explorer_open);
   _commands.add("show.world_stats"s, _world_stats_open);

   _commands.add("save"s, [this]() { save_world(_world_path); });

   _commands.add("entity_edit.ground_objects"s, _selection_edit_context.ground_objects);
   _commands.add("entity_edit.move_selection"s, [this] {
      _selection_edit_tool = selection_edit_tool::move;
      _move_selection_amount = {0.0f, 0.0f, 0.0f};
   });
   _commands.add("entity_edit.rotate_selection"s, [this] {
      _selection_edit_tool = selection_edit_tool::rotate;
      _rotate_selection_amount = {0.0f, 0.0f, 0.0f};
   });
   _commands.add("entity_edit.align_selection"s, [this] { align_selection(); });
   _commands.add("entity_edit.new_from_selection"s,
                 [this] { new_entity_from_selection(); });
   _commands.add("entity_edit.open_odf"s, [this] { open_odfs_for_selected(); });

   _commands.add("entity_creation.cycle_rotation_mode"s, [this] {
      switch (_entity_creation_config.placement_rotation) {
      case placement_rotation::manual_euler:
         _entity_creation_config.placement_rotation =
            placement_rotation::manual_quaternion;
         return;
      case placement_rotation::manual_quaternion:
         _entity_creation_config.placement_rotation = placement_rotation::surface;
         return;
      case placement_rotation::surface:
         _entity_creation_config.placement_rotation = placement_rotation::manual_euler;
         return;
      }
   });
   _commands.add("entity_creation.cycle_placement_mode"s, [this] {
      switch (_entity_creation_config.placement_mode) {
      case placement_mode::manual:
         _entity_creation_config.placement_mode = placement_mode::cursor;
         return;
      case placement_mode::cursor:
         _entity_creation_config.placement_mode = placement_mode::manual;
         return;
      }
   });
   _commands.add("entity_creation.cycle_alignment_mode"s, [this] {
      switch (_entity_creation_config.placement_alignment) {
      case placement_alignment::none:
         _entity_creation_config.placement_alignment = placement_alignment::grid;
         return;
      case placement_alignment::grid:
         _entity_creation_config.placement_alignment = placement_alignment::snapping;
         return;
      case placement_alignment::snapping:
         _entity_creation_config.placement_alignment = placement_alignment::none;
         return;
      }
   });
   _commands.add("entity_creation.cycle_ground_mode"s, [this] {
      switch (_entity_creation_config.placement_ground) {
      case placement_ground::origin:
         _entity_creation_config.placement_ground = placement_ground::bbox;
         return;
      case placement_ground::bbox:
         _entity_creation_config.placement_ground = placement_ground::origin;
         return;
      }
   });
   _commands.add("entity_creation.cycle_object_class"s,
                 [this] { cycle_creation_entity_object_class(); });

   _commands.add("entity_creation.activate_point_at"s,
                 _entity_creation_context.activate_point_at);
   _commands.add("entity_creation.deactivate_point_at"s,
                 [this] { _entity_creation_context.using_point_at = false; });
   _commands.add("entity_creation.activate_extend_to"s,
                 _entity_creation_context.activate_extend_to);
   _commands.add("entity_creation.activate_shrink_to"s,
                 _entity_creation_context.activate_shrink_to);
   _commands.add("entity_creation.deactivate_resize_to"s, [this] {
      _entity_creation_context.using_shrink_to = false;
      _entity_creation_context.using_extend_to = false;
   });
   _commands.add("entity_creation.activate_from_object_bbox"s,
                 _entity_creation_context.activate_from_object_bbox);
   _commands.add("entity_creation.finish_from_object_bbox"s, [this] {
      _entity_creation_context.finish_from_object_bbox = true;
   });

   _commands.add("entity_creation.activate_from_line"s,
                 _entity_creation_context.activate_from_line);
   _commands.add("entity_creation.deactivate_from_line"s, [this] {
      _entity_creation_context.using_from_line = false;
      _entity_creation_context.from_line_click = false;
   });
   _commands.add("entity_creation.from_line_click"s,
                 _entity_creation_context.from_line_click);

   _commands.add("entity_creation.activate_draw_barrier"s,
                 _entity_creation_context.activate_draw_barrier);
   _commands.add("entity_creation.deactivate_draw_barrier"s, [this] {
      _entity_creation_context.using_draw_barrier = false;
      _entity_creation_context.draw_barrier_click = false;
   });

   _commands.add("entity_creation.deactivate_pick_sector"s,
                 [this] { _entity_creation_context.using_pick_sector = false; });

   _commands.add("entity_creation.draw_barrier_click"s,
                 _entity_creation_context.draw_barrier_click);

   _commands.add("entity_creation.lock_x_axis"s, _entity_creation_context.lock_x_axis);
   _commands.add("entity_creation.lock_y_axis"s, _entity_creation_context.lock_y_axis);
   _commands.add("entity_creation.lock_z_axis"s, _entity_creation_context.lock_z_axis);

   _commands.add("entity_creation.finish_path"s,
                 _entity_creation_context.finish_current_path);

   _commands.add("entity_creation.rotate_forward"s,
                 _entity_creation_context.rotate_forward);
   _commands.add("entity_creation.rotate_back"s, _entity_creation_context.rotate_back);

   _commands.add("entity_creation.place"s, [this] { place_creation_entity(); });
   _commands.add("entity_creation.cancel"s, [this] {
      if (not _interaction_targets.creation_entity) return;

      _edit_stack_world
         .apply(edits::make_creation_entity_set(std::nullopt,
                                                _interaction_targets.creation_entity),
                _edit_context);
   });
}

void world_edit::initialize_hotkeys() noexcept
{
   _hotkeys.add_set(
      {.name = "",
       .description = "Global hotkeys. These are always active.\n\nThese "
                      "bindings have a lower priority than any other set."s,
       .activated = [] { return true; },
       .default_hotkeys = {
          {"Move Forward", "camera.move_forward", {.key = key::w}, {.toggle = true}},
          {"Move Back", "camera.move_back", {.key = key::s}, {.toggle = true}},
          {"Move Left", "camera.move_left", {.key = key::a}, {.toggle = true}},
          {"Move Right", "camera.move_right", {.key = key::d}, {.toggle = true}},
          {"Move Up", "camera.move_up", {.key = key::r}, {.toggle = true}},
          {"Move Down", "camera.move_down", {.key = key::f}, {.toggle = true}},
          {"Move Sprint Forward",
           "camera.move_sprint_forward",
           {.key = key::w, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Move Sprint Back",
           "camera.move_sprint_back",
           {.key = key::s, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Move Sprint Left",
           "camera.move_sprint_left",
           {.key = key::a, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Move Sprint Right",
           "camera.move_sprint_right",
           {.key = key::d, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Move Sprint Up",
           "camera.move_sprint_up",
           {.key = key::r, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Move Sprint Down",
           "camera.move_sprint_down",
           {.key = key::f, .modifiers = {.shift = true}},
           {.toggle = true}},
          {"Rotate Camera", "camera.rotate_with_mouse", {.key = key::mouse2}, {.toggle = true}},
          {"Pan Camera",
           "camera.pan_with_mouse",
           {.key = key::mouse1, .modifiers = {.alt = true}},
           {.toggle = true}},
          {"Zoom In",
           "camera.zoom_in",
           {.key = key::mouse_wheel_forward, .modifiers = {.ctrl = true}}},
          {"Zoom Out",
           "camera.zoom_out",
           {.key = key::mouse_wheel_back, .modifiers = {.ctrl = true}}},
          {"Step Forward",
           "camera.step_forward",
           {.key = key::mouse_wheel_forward, .modifiers = {.alt = true}}},
          {"Step Back",
           "camera.step_back",
           {.key = key::mouse_wheel_back, .modifiers = {.alt = true}}},

          {"Set Perspective Camera", "camera.set_perspective", {.key = key::p}},
          {"Set Orthographic Camera", "camera.set_orthographic", {.key = key::o}},

          {"Select", "selection.set", {.key = key::mouse1}},
          {"Select Multiple", "selection.add", {.key = key::mouse1, .modifiers = {.shift = true}}},
          {"Deselect", "selection.remove", {.key = key::mouse1, .modifiers = {.ctrl = true}}},
          {"Clear Selection", "selection.clear", {.key = key::escape}},

          {"Undo", "edit.undo", {.key = key::z, .modifiers = {.ctrl = true}}},
          {"Redo", "edit.redo", {.key = key::y, .modifiers = {.ctrl = true}}},
          {"Delete", "edit.delete", {.key = key::del}},

          {"Show Hotkeys", "show.hotkeys", {.key = key::f1}},
          {"Show Camera Controls", "show.camera_controls", {.key = key::f2}},

          {"Show World Global Lights Editor",
           "show.world_global_lights_editor",
           {.key = key::f4}},
          {"Show World Layers Editor", "show.world_layers_editor", {.key = key::f5}},
          {"Show World Game Mode Editor", "show.world_game_mode_editor", {.key = key::f6}},
          {"Show World Requirements Editor", "show.world_requirements_editor", {.key = key::f7}},
          {"Show World Explorer", "show.world_explorer", {.key = key::f8}},
          {"Show World Stats", "show.world_stats", {.key = key::f9}},

          {"Save", "save", {.key = key::s, .modifiers = {.ctrl = true}}, {.ignore_imgui_focus = true}},
       }});

   _hotkeys.add_set(
      {.name = "Entity Editing",
       .description = "Hotkeys for editing already existing entities.\n\nThese are active whenever entities are selected. Their bindings have lower priority than Entity Creation bindings."s,
       .activated = [this] { return not _interaction_targets.selection.empty(); },
       .default_hotkeys{
          {"Move Selection", "entity_edit.move_selection", {.key = key::z}},
          {"Rotate Selection", "entity_edit.rotate_selection", {.key = key::x}},
          {"Ground Objects", "entity_edit.ground_objects", {.key = key::g}},
          {"Align Selection (Terrain Grid)",
           "entity_edit.align_selection",
           {.key = key::a, .modifiers = {.shift = true}}},
          {"New Entity from Selection", "entity_edit.new_from_selection", {.key = key::n}},
          {"Open .odf in Text Editor",
           "entity_edit.open_odf",
           {.key = key::o, .modifiers = {.ctrl = true}}},
       }});

   _hotkeys.add_set(
      {.name = "Entity Creation",
       .description = "Hotkeys for creating new entities.\n\nThese are active while a new entity is being created. Their bindings currently only have lower priority than tool bindings. Like Point At."s,
       .activated =
          [this] { return _interaction_targets.creation_entity.has_value(); },
       .default_hotkeys = {
          {"Change Rotation Mode",
           "entity_creation.cycle_rotation_mode",
           {.key = key::q, .modifiers = {.ctrl = true}}},
          {"Change Placement Mode",
           "entity_creation.cycle_placement_mode",
           {.key = key::w, .modifiers = {.ctrl = true}}},
          {"Change Alignment Mode",
           "entity_creation.cycle_alignment_mode",
           {.key = key::e, .modifiers = {.ctrl = true}}},
          {"Change Grounding Mode",
           "entity_creation.cycle_ground_mode",
           {.key = key::r, .modifiers = {.ctrl = true}}},
          {"Cycle Object Class", "entity_creation.cycle_object_class", {.key = key::q}},

          {"Start Point At", "entity_creation.activate_point_at", {.key = key::v}},
          {"Start Extend To", "entity_creation.activate_extend_to", {.key = key::t}},
          {"Start Shrink To",
           "entity_creation.activate_shrink_to",
           {.key = key::t, .modifiers = {.ctrl = true}}},
          {"Start From Object BBOX",
           "entity_creation.activate_from_object_bbox",
           {.key = key::b}},
          {"Start From Line",
           "entity_creation.activate_from_line",
           {.key = key::f, .modifiers = {.ctrl = true}}},
          {"Start Draw Barrier",
           "entity_creation.activate_draw_barrier",
           {.key = key::d, .modifiers = {.ctrl = true}}},

          {"Lock X Axis", "entity_creation.lock_x_axis", {.key = key::z}},
          {"Lock Y Axis", "entity_creation.lock_y_axis", {.key = key::x}},
          {"Lock Z Axis", "entity_creation.lock_z_axis", {.key = key::c}},

          {"Rotate Entity Forward", "entity_creation.rotate_forward", {.key = key::mouse_wheel_forward}},
          {"Rotate Entity Back", "entity_creation.rotate_back", {.key = key::mouse_wheel_back}},

          {"Place Entity", "entity_creation.place", {.key = key::mouse1}},
          {"Cancel", "entity_creation.cancel", {.key = key::escape}},

          {"Finish Path", "entity_creation.finish_path", {.key = key::g}},
       }});

   _hotkeys.add_set({
      .name = "Entity Creation (Point At)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   _entity_creation_context.using_point_at;
         },
      .default_hotkeys =
         {
            {"Stop Point At", "entity_creation.deactivate_point_at", {.key = key::mouse1}},
            {"Stop Point At (Escape)", "entity_creation.deactivate_point_at", {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Entity Creation (Resize To)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   (_entity_creation_context.using_extend_to or
                    _entity_creation_context.using_shrink_to);
         },
      .default_hotkeys =
         {
            {"Stop Resize To", "entity_creation.deactivate_resize_to", {.key = key::mouse1}},
            {"Stop Resize To (Escape)",
             "entity_creation.deactivate_resize_to",
             {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Entity Creation (From BBOX)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   _entity_creation_context.using_from_object_bbox;
         },
      .default_hotkeys =
         {
            {"Complete From Object BBOX",
             "entity_creation.finish_from_object_bbox",
             {.key = key::mouse1}},
            {"Complete From Object BBOX (Escape)",
             "entity_creation.finish_from_object_bbox",
             {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Entity Creation (From Line)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   _entity_creation_context.using_from_line;
         },
      .default_hotkeys =
         {
            {"Stop From Line", "entity_creation.from_line_click", {.key = key::mouse1}},
            {"Stop From Line (Escape)",
             "entity_creation.deactivate_from_line",
             {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Entity Creation (Draw Barrier)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   _entity_creation_context.using_draw_barrier;
         },
      .default_hotkeys =
         {
            {"Draw Barrier", "entity_creation.draw_barrier_click", {.key = key::mouse1}},
            {"Stop Draw Barrier", "entity_creation.deactivate_draw_barrier", {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Entity Creation (Pick Sector)",
      .activated =
         [this] {
            return _interaction_targets.creation_entity and
                   _entity_creation_context.using_pick_sector;
         },
      .default_hotkeys =
         {
            {"Stop Pick Sector", "entity_creation.deactivate_pick_sector", {.key = key::mouse1}},
            {"Stop Pick Sector (Escape)",
             "entity_creation.deactivate_pick_sector",
             {.key = key::escape}},
         },

      .hidden = true,
   });

   _hotkeys.add_set({
      .name = "Camera Active Controls",
      .description = "Controls for increasing camera movement speed. Active while Rotate Camera is being used."s,
      .activated = [this] { return _rotate_camera; },
      .default_hotkeys =
         {
            {"Increase Move Speed", "camera.double_move_speed", {.key = key::mouse_wheel_forward}},
            {"Decrease Move Speed", "camera.halve_move_speed", {.key = key::mouse_wheel_back}},
         },
   });
}

}