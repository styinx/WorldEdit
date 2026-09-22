#pragma once

#include "edit.hpp"

#include "world/interaction_context.hpp"

#include <memory>

namespace we::edits {

namespace {

template<typename T>
struct add_vector_entry final : edit<world::edit_context> {
   add_vector_entry(std::vector<T>* vector, T value)
      : _vector{vector}, _value{std::move(value)}
   {
   }

   void apply([[maybe_unused]] world::edit_context& context) noexcept override
   {
      assert(context.is_memory_valid(_vector));

      _vector->push_back(std::move(_value));
   }

   void revert([[maybe_unused]] world::edit_context& context) noexcept override
   {
      assert(context.is_memory_valid(_vector));

      using std::swap;

      swap(_value, _vector->back());

      _vector->pop_back();
   }

   bool is_coalescable([[maybe_unused]] const edit& other) const noexcept override
   {
      return false;
   }

   void coalesce([[maybe_unused]] edit& other) noexcept override {}

private:
   std::vector<T>* _vector;
   T _value;
};

}

template<typename T>
auto make_add_vector_entry(std::vector<T>* vector, T value)
   -> std::unique_ptr<edit<world::edit_context>>
{
   return std::make_unique<add_vector_entry<T>>(vector, std::move(value));
}

}