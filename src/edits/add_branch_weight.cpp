#include "add_branch_weight.hpp"

#include "add_vector_entry.hpp"

namespace we::edits {

auto make_add_branch_weight(std::vector<world::planning_branch_weights>* branch_weights,
                            world::planning_branch_weights weights)
   -> std::unique_ptr<edit<world::edit_context>>
{
   return make_add_vector_entry(branch_weights, weights);
}

}
