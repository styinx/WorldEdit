#include "add_animation_hierarchy_child.hpp"

#include "add_vector_entry.hpp"

namespace we::edits {

auto make_add_animation_hierarchy_child(std::vector<uint32>* children, uint32 new_child)
   -> std::unique_ptr<edit<world::edit_context>>
{
   return make_add_vector_entry(children, new_child);
}

}
