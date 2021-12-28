
#include "assets/asset_libraries.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer.hpp"
#include "input_state.hpp"
#include "output_stream.hpp"
#include "settings/settings.hpp"
#include "utility/command_line.hpp"
#include "utility/synchronous_task_queue.hpp"
#include "world/object_class.hpp"
#include "world/world.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

#include <Windows.h>

#include <absl/container/flat_hash_map.h>

struct ImGuiContext;

namespace we {

class world_edit {
public:
   world_edit(const HWND window, utility::command_line command_line);

   bool update();

   void resized(uint16 width, uint16 height);

   void focused();

   void unfocused();

   bool idling() const noexcept;

   void mouse_wheel_movement(const float movement) noexcept;

   void update_cursor() noexcept;

   void char_input(const char16_t c) noexcept;

   void dpi_changed(const int new_dpi) noexcept;

private:
   void update_object_classes();

   void update_camera(const float delta_time, const mouse_state& mouse_state,
                      const keyboard_state& keyboard_state);

   void update_ui(const mouse_state& mouse_state,
                  const keyboard_state& keyboard_state) noexcept;

   void object_definition_loaded(const lowercase_string& name,
                                 asset_ref<assets::odf::definition> asset,
                                 asset_data<assets::odf::definition> data);

   void model_loaded(const lowercase_string& name,
                     asset_ref<assets::msh::flat_model> asset,
                     asset_data<assets::msh::flat_model> data);

   void open_project(std::filesystem::path path) noexcept;

   void open_project_with_picker() noexcept;

   void load_world(std::filesystem::path path) noexcept;

   void load_world_with_picker() noexcept;

   void save_world(std::filesystem::path path) noexcept;

   void save_world_with_picker() noexcept;

   void close_world() noexcept;

   void enumerate_project_worlds() noexcept;

   standard_output_stream _stream;
   HWND _window{};
   std::shared_ptr<settings::settings> _settings =
      std::make_shared<settings::settings>();

   bool _focused = true;
   float _current_dpi = 96.0f;
   float _display_scale = 1.0f;
   std::chrono::steady_clock::time_point _last_update =
      std::chrono::steady_clock::now();

   std::unique_ptr<ImGuiContext, void (*)(ImGuiContext*)> _imgui_context;

   std::filesystem::path _project_dir;
   std::filesystem::path _world_path;

   std::vector<std::filesystem::path> _project_world_paths;

   assets::libraries_manager _asset_libraries{_stream};
   absl::flat_hash_map<lowercase_string, std::shared_ptr<world::object_class>> _object_classes;
   world::world _world;

   graphics::renderer _renderer;
   graphics::controllable_perspective_camera _camera;

   utility::synchronous_task_queue _asset_load_queue;
   event_listener<void(const lowercase_string&, asset_ref<assets::odf::definition>,
                       asset_data<assets::odf::definition>)>
      _object_definition_load_listener = _asset_libraries.odfs.listen_for_loads(
         [this](lowercase_string name, asset_ref<assets::odf::definition> asset,
                asset_data<assets::odf::definition> data) {
            _asset_load_queue.enqueue(
               [=] { object_definition_loaded(name, asset, data); });
         });
   event_listener<void(const lowercase_string&, asset_ref<assets::msh::flat_model>,
                       asset_data<assets::msh::flat_model>)>
      _model_load_listener = _asset_libraries.models.listen_for_loads(
         [this](lowercase_string name, asset_ref<assets::msh::flat_model> asset,
                asset_data<assets::msh::flat_model> data) {
            _asset_load_queue.enqueue([=] { model_loaded(name, asset, data); });
         });
};

}
