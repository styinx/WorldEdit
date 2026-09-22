#include "add_animation_group_entry.hpp"

#include "add_vector_entry.hpp"

namespace we::edits {

auto make_add_animation_group_entry(std::vector<world::animation_group::entry>* entries,
                                    world::animation_group::entry new_entry)
   -> std::unique_ptr<edit<world::edit_context>>
{
   return make_add_vector_entry(entries, new_entry);
}

}