#pragma once

#include "types.hpp"

#include "utility/implementation_storage.hpp"

#include <span>
#include <string_view>

namespace we::world {

struct model;

enum class model_handle : uint32 {};

struct model_library {
   const static uint32 max_models;

   enum class event_type {
      /// @brief The library has been cleared.
      cleared,
      /// @brief A model has been added to the library.
      model_added,
      /// @brief A model has been removed from the library.
      model_removed,
   };

   struct event {
      /// @brief The type of the event.
      event_type type;

      /// @brief Handle of the added mesh for event_type::new_mesh and event_type::mesh_removed.
      /// For event_type::mesh_removed the handle is no longer valid.
      model_handle handle = {};

      bool operator==(const event&) const noexcept = default;
   };

   /// @brief Gets events cataloging changes to the library.
   /// @return The events for the current frame.
   auto events() const noexcept -> std::span<const event>;

   /// @brief Clear the event queue.
   void clear_events() noexcept;

   /// @brief Issue events needed to restore the GPU copy of the library.
   void issue_restore_events() noexcept;

   /// @brief Gets a model from a handle.
   /// @param handle The handle to the model.
   /// @return A const reference to the model or a reference to the default model.
   auto operator[](const model_handle handle) const noexcept -> const model&;

   /// @brief Acquire a model handle for the model name or a handle to the default model if the max model count has been reached.
   /// @param name The name of the model to get a handle for.
   /// @return The handle. This must always be passed to free when you are done with it.
   [[nodiscard]] auto acquire(const std::string_view name) noexcept -> model_handle;

   /// @brief Free a model handle.
   /// @param handle The handle to free.
   void free(const model_handle handle) noexcept;

   /// @brief Gets the current size of the library's pool. Used to duplicate the library for the GPU.
   /// @return The current size of the pool.
   auto pool_size() const noexcept -> uint32;

   /// @brief Converts a handle to it's pool index.
   /// @return The index of the handle.
   static auto unpack_pool_index(const model_handle handle) noexcept -> uint32;

   /// @brief Returns the null handle. When passed to operator[] this will return an empty mesh. Passing the null handle to remove is a no-op.
   /// @return The null handle.
   static auto null_handle() noexcept -> model_handle;

   /// @brief Gets the number of references to custom mesh. For testing and debugging.
   /// @return The ref count.
   auto debug_ref_count(const model_handle& mesh,
                        bool exclude_event_queue_ref = true) const noexcept -> uint32;

   /// @brief Gets a handle to a model without increasing it's ref count. For testing and debugging.
   /// @return The handle or the null handle.
   auto debug_query_handle(const std::string_view name) const noexcept -> model_handle;

   model_library();

   model_library(const model_library&);
   auto operator=(const model_library&) noexcept -> model_library&;

   model_library(model_library&&) noexcept;
   auto operator=(model_library&&) noexcept -> model_library&;

   ~model_library();

private:
   struct impl;

   implementation_storage<impl, 128> _impl;
};

}