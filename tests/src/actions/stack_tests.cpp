#include "pch.h"

#include "actions/stack.hpp"
#include "world/world.hpp"

using namespace std::literals;

namespace we::actions::tests {

namespace {

struct dummy_action : action {
   dummy_action(int& apply_call_count, int& revert_call_count)
      : apply_call_count{apply_call_count}, revert_call_count{revert_call_count}
   {
   }

   void apply(world::world&) noexcept override
   {
      ++apply_call_count;
   }

   void revert(world::world&) noexcept override
   {
      ++revert_call_count;
   }

   int& apply_call_count;
   int& revert_call_count;
};

struct dummy_ordering_action : action {
   dummy_ordering_action(bool& toggle) : toggle{toggle} {}

   void apply(world::world&) noexcept override
   {
      toggle = not toggle;
   }

   void revert(world::world&) noexcept override
   {
      toggle = not toggle;
   }

   bool& toggle;
};

}

TEST_CASE("actions stack core tests", "[Actions]")
{
   stack stack;
   world::world world;

   int apply_call_count = 0;
   int revert_call_count = 0;

   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);

   REQUIRE(apply_call_count == 1);
   REQUIRE(revert_call_count == 0);
   REQUIRE(stack.applied_size() == 1);
   REQUIRE(stack.reverted_size() == 0);

   stack.revert(world);

   REQUIRE(apply_call_count == 1);
   REQUIRE(revert_call_count == 1);
   REQUIRE(stack.applied_size() == 0);
   REQUIRE(stack.reverted_size() == 1);

   stack.reapply(world);

   REQUIRE(apply_call_count == 2);
   REQUIRE(revert_call_count == 1);
   REQUIRE(stack.applied_size() == 1);
   REQUIRE(stack.reverted_size() == 0);
}

TEST_CASE("actions stack count function tests", "[Actions]")
{
   stack stack;
   world::world world;

   int apply_call_count = 0;
   int revert_call_count = 0;

   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);

   REQUIRE(apply_call_count == 3);
   REQUIRE(revert_call_count == 0);
   REQUIRE(stack.applied_size() == 3);
   REQUIRE(stack.reverted_size() == 0);

   stack.revert(2, world);

   REQUIRE(apply_call_count == 3);
   REQUIRE(revert_call_count == 2);
   REQUIRE(stack.applied_size() == 1);
   REQUIRE(stack.reverted_size() == 2);

   stack.revert(2, world);

   REQUIRE(apply_call_count == 3);
   REQUIRE(revert_call_count == 3);
   REQUIRE(stack.applied_size() == 0);
   REQUIRE(stack.reverted_size() == 3);

   stack.reapply(2, world);

   REQUIRE(apply_call_count == 5);
   REQUIRE(revert_call_count == 3);
   REQUIRE(stack.applied_size() == 2);
   REQUIRE(stack.reverted_size() == 1);

   stack.reapply(2, world);

   REQUIRE(apply_call_count == 6);
   REQUIRE(revert_call_count == 3);
   REQUIRE(stack.applied_size() == 3);
   REQUIRE(stack.reverted_size() == 0);
}

TEST_CASE("actions stack _all function tests", "[Actions]")
{
   stack stack;
   world::world world;

   int apply_call_count = 0;
   int revert_call_count = 0;

   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);

   REQUIRE(apply_call_count == 3);
   REQUIRE(revert_call_count == 0);
   REQUIRE(stack.applied_size() == 3);
   REQUIRE(stack.reverted_size() == 0);

   stack.revert_all(world);

   REQUIRE(apply_call_count == 3);
   REQUIRE(revert_call_count == 3);
   REQUIRE(stack.applied_size() == 0);
   REQUIRE(stack.reverted_size() == 3);

   stack.reapply_all(world);

   REQUIRE(apply_call_count == 6);
   REQUIRE(revert_call_count == 3);
   REQUIRE(stack.applied_size() == 3);
   REQUIRE(stack.reverted_size() == 0);
}

TEST_CASE("actions stack ordering tests", "[Actions]")
{
   stack stack;
   world::world world;

   bool a_active = false;
   bool b_active = false;
   bool c_active = false;

   stack.apply(std::make_unique<dummy_ordering_action>(a_active), world);
   stack.apply(std::make_unique<dummy_ordering_action>(b_active), world);
   stack.apply(std::make_unique<dummy_ordering_action>(c_active), world);

   REQUIRE(a_active);
   REQUIRE(b_active);
   REQUIRE(c_active);

   stack.revert(2, world);

   REQUIRE(a_active);
   REQUIRE(not b_active);
   REQUIRE(not c_active);

   stack.reapply(1, world);

   REQUIRE(a_active);
   REQUIRE(b_active);
   REQUIRE(not c_active);
}

TEST_CASE("actions stack empty function tests", "[Actions]")
{
   stack stack;
   world::world world;

   REQUIRE(stack.applied_empty());
   REQUIRE(stack.reverted_empty());

   int apply_call_count = 0;
   int revert_call_count = 0;

   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);
   stack.apply(std::make_unique<dummy_action>(apply_call_count, revert_call_count),
               world);

   REQUIRE(not stack.applied_empty());
   REQUIRE(stack.reverted_empty());

   stack.revert(2, world);

   REQUIRE(not stack.applied_empty());
   REQUIRE(not stack.reverted_empty());

   stack.revert(1, world);

   REQUIRE(stack.applied_empty());
   REQUIRE(not stack.reverted_empty());
}

TEST_CASE("actions stack applied_top", "[Actions]")
{
   stack stack;
   world::world world;

   REQUIRE(stack.applied_top() == nullptr);

   int apply_call_count = 0;
   int revert_call_count = 0;

   auto unique_action =
      std::make_unique<dummy_action>(apply_call_count, revert_call_count);
   auto* action = unique_action.get();

   stack.apply(std::move(unique_action), world);

   REQUIRE(stack.applied_top() == action);
}

}