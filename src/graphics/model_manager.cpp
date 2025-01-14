
#include "model_manager.hpp"

#include "assets/msh/default_missing_scene.hpp"
#include "lowercase_string.hpp"
#include "utility/string_ops.hpp"

#include <fmt/core.h>

namespace we::graphics {

model_manager::model_manager(gpu::device& gpu_device,
                             copy_command_list_pool& copy_command_list_pool,
                             texture_manager& texture_manager,
                             assets::library<assets::msh::flat_model>& model_assets,
                             std::shared_ptr<async::thread_pool> thread_pool,
                             output_stream& error_output)
   : _gpu_device{gpu_device},
     _copy_command_list_pool{copy_command_list_pool},
     _model_assets{model_assets},
     _texture_manager{texture_manager},
     _thread_pool{thread_pool},
     _placeholder_model{*assets::msh::default_missing_scene(), _gpu_device,
                        copy_command_list_pool, _texture_manager},
     _error_output{error_output}
{
}

auto model_manager::operator[](const lowercase_string& name) -> model&
{
   if (name.empty()) return _placeholder_model;

   // Check to see if the model is already loaded and ready.
   {
      std::shared_lock lock{_mutex};

      if (auto state = _models.find(name); state != _models.end()) {
         return *state->second.model;
      }

      if (_pending_creations.contains(name) or _failed_creations.contains(name)) {
         return _placeholder_model;
      }
   }

   auto asset = _model_assets[name];

   if (not asset.exists()) return _placeholder_model;

   std::scoped_lock lock{_mutex};

   // Make sure another thread hasn't created the model inbetween us checking
   // for it and taking the write lock.
   if (_pending_creations.contains(name)) return _placeholder_model;

   if (auto state = _models.find(name); state != _models.end()) {
      return *state->second.model;
   }

   auto flat_model = asset.get_if();

   if (flat_model == nullptr) return _placeholder_model;

   enqueue_create_model(name, asset, flat_model);

   return _placeholder_model;
}

void model_manager::update_models() noexcept
{
   std::scoped_lock lock{_mutex};

   for (auto it = _pending_creations.begin(); it != _pending_creations.end();) {
      auto elem_it = it++;
      auto& [name, pending_create] = *elem_it;

      if (pending_create.task.ready()) {
         try {
            // TODO: Currently we don't have to worry about model lifetime here but if in the future we're rendering
            // thumbnails in the background we may have to revist this.
            _models[name] = pending_create.task.get();
         }
         catch (std::exception& e) {
            _error_output.write(
               "Failed to create model:\n   Name: {}\n   Message: \n{}\n",
               std::string_view{name}, string::indent(2, e.what()));

            _failed_creations.insert(name);
         }

         _pending_creations.erase(elem_it);
      }
   }
}

void model_manager::trim_models() noexcept
{
   std::lock_guard lock{_mutex};

   erase_if(_models, [](const auto& key_value) {
      const auto& [name, state] = key_value;

      return state.asset.use_count() == 1;
   });

   _pending_destroys.clear();
}

void model_manager::model_loaded(const lowercase_string& name,
                                 asset_ref<assets::msh::flat_model> asset,
                                 asset_data<assets::msh::flat_model> data) noexcept
{
   std::scoped_lock lock{_mutex};

   if (_pending_creations.contains(name)) {
      _pending_creations[name].task.cancel();
   }

   _failed_creations.erase(name);

   enqueue_create_model(name, asset, data);
}

void model_manager::enqueue_create_model(const lowercase_string& name,
                                         asset_ref<assets::msh::flat_model> asset,
                                         asset_data<assets::msh::flat_model> flat_model) noexcept
{
   _pending_creations[name] =
      {.task =
          _thread_pool->exec(async::task_priority::low,
                             [this, name = name, asset, flat_model]() -> model_state {
                                auto new_model =
                                   std::make_unique<model>(*flat_model, _gpu_device,
                                                           _copy_command_list_pool,
                                                           _texture_manager);

                                _gpu_device.background_copy_queue.wait_for_idle();

                                std::shared_lock lock{_mutex};

                                // Make sure an asset load event hasn't loaded a new asset before us.
                                // This stops us replacing a new asset with an out of date one.
                                if (not _pending_creations.contains(name) or
                                    _pending_creations[name].flat_model != flat_model) {
                                   return {};
                                }

                                return {.model = std::move(new_model), .asset = asset};
                             }),
       .flat_model = flat_model};
}

}
