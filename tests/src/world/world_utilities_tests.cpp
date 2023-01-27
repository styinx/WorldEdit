#include "pch.h"

#include "world/world.hpp"
#include "world/world_utilities.hpp"

#include <string_view>

using namespace std::literals;

namespace we::world::tests {

TEST_CASE("world utilities find_entity", "[World][Utilities]")
{
   world world{.regions = {region{.name = "some_region"s, .description = "some_desc"s}}};

   REQUIRE(find_entity(world.regions, "some_region"sv) == &world.regions[0]);
   REQUIRE(find_entity(world.regions, "no_region"sv) == nullptr);
}

TEST_CASE("world utilities find_region", "[World][Utilities]")
{
   world world{.regions = {region{.name = "some_region"s, .description = "some_desc"s}}};

   REQUIRE(find_region(world, "some_region"sv) == &world.regions[0]);
   REQUIRE(find_region(world, "no_region"sv) == nullptr);
}

TEST_CASE("world utilities find_region_by_description", "[World][Utilities]")
{
   world world{.regions = {region{.name = "some_region"s, .description = "some_desc"s}}};

   REQUIRE(find_region_by_description(world, "some_desc"sv) == &world.regions[0]);
   REQUIRE(find_region_by_description(world, "no_desc"sv) == nullptr);
}

TEST_CASE("world utilities create_unique_name", "[World][Utilities]")
{
   world world{.objects = {object{.name = "Amazing Object 32"s}, object{.name = "62"s}}};

   REQUIRE(create_unique_name(world.objects, "Amazing Object 32") ==
           "Amazing Object 0");
   REQUIRE(create_unique_name(world.objects, "Amazing Object") ==
           "Amazing Object");
   REQUIRE(create_unique_name(world.objects, "62") == "Object0");
   REQUIRE(create_unique_name(world.objects, "63") == "63");
   REQUIRE(create_unique_name(world.objects, "") == "");
}

}
