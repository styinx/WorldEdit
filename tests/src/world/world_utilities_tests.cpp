#include "pch.h"

#include "world/utility/world_utilities.hpp"
#include "world/world.hpp"

#include <string_view>

using namespace std::literals;

namespace we::world::tests {

TEST_CASE("world utilities find_entity by name", "[World][Utilities]")
{
   world world{.regions = {region{.name = "some_region"s, .description = "some_desc"s}}};

   REQUIRE(find_entity(world.regions, "some_region"sv) == &world.regions[0]);
   REQUIRE(find_entity(world.regions, "no_region"sv) == nullptr);
}

TEST_CASE("world utilities find_entity by id", "[World][Utilities]")
{
   world world;

   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   auto missing_id = world.next_id.regions.aquire();
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});

   REQUIRE(find_entity(world.regions, world.regions[0].id) == &world.regions[0]);
   REQUIRE(find_entity(world.regions, world.regions[1].id) == &world.regions[1]);
   REQUIRE(find_entity(world.regions, world.regions[2].id) == &world.regions[2]);
   REQUIRE(find_entity(world.regions, world.regions[3].id) == &world.regions[3]);
   REQUIRE(find_entity(world.regions, world.regions[4].id) == &world.regions[4]);
   REQUIRE(find_entity(world.regions, missing_id) == nullptr);

   world.regions.clear();

   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});
   world.regions.push_back(region{.id = world.next_id.regions.aquire()});

   REQUIRE(find_entity(world.regions, world.regions[0].id) == &world.regions[0]);
   REQUIRE(find_entity(world.regions, world.regions[1].id) == &world.regions[1]);
   REQUIRE(find_entity(world.regions, world.regions[2].id) == &world.regions[2]);
   REQUIRE(find_entity(world.regions, world.regions[3].id) == &world.regions[3]);
   REQUIRE(find_entity(world.regions, world.regions[4].id) == &world.regions[4]);
   REQUIRE(find_entity(world.regions, missing_id) == nullptr);
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
   REQUIRE(create_unique_name(world.objects, "Amazing Object 31") ==
           "Amazing Object 31");
   REQUIRE(create_unique_name(world.objects, "62") == "Object0");
   REQUIRE(create_unique_name(world.objects, "63") == "63");
   REQUIRE(create_unique_name(world.objects, "") == "");
}

TEST_CASE("world utilities create_unique_light_region_name",
          "[World][Utilities]")
{
   world world{
      .lights = {light{.name = "Light0"s}},
      .regions = {region{.name = "Region0"s}},
   };

   REQUIRE(create_unique_light_region_name(world.lights, world.regions, "Light0") ==
           "Light1");
   REQUIRE(create_unique_light_region_name(world.lights, world.regions,
                                           "Region0") == "Region1");
   REQUIRE(create_unique_light_region_name(world.lights, world.regions, "") ==
           "LightRegion0");
   REQUIRE(create_unique_light_region_name(world.lights, world.regions,
                                           "Region1") == "Region1");
}

TEST_CASE("world utilities find_closest_node", "[World][Utilities]")
{
   path path{.nodes = {
                path::node{.position = {0.0f, 0.0f, 0.0f}},
                path::node{.position = {0.0f, 1.0f, 0.0f}},
                path::node{.position = {0.0f, 2.0f, 0.0f}},
                path::node{.position = {0.0f, 4.0f, 0.0f}},
             }};

   REQUIRE(find_closest_node({0.0f, 0.9f, 0.0f}, path).index == 1);
   REQUIRE(not find_closest_node({0.0f, 0.9f, 0.0f}, path).next_is_forward);

   REQUIRE(find_closest_node({0.0f, 1.1f, 0.0f}, path).index == 1);
   REQUIRE(find_closest_node({0.0f, 1.1f, 0.0f}, path).next_is_forward);
}

TEST_CASE("world utilities find_closest_point", "[World][Utilities]")
{
   sector sector{.points = {
                    {0.0f, 0.0f},
                    {0.0f, 1.0f},
                    {0.0f, 2.0f},
                    {0.0f, 4.0f},
                 }};

   REQUIRE(find_closest_point({0.0f, 0.9f}, sector).index == 1);
   REQUIRE(not find_closest_point({0.0f, 0.9f}, sector).next_is_forward);

   REQUIRE(find_closest_point({0.0f, 1.1f}, sector).index == 1);
   REQUIRE(find_closest_point({0.0f, 1.1f}, sector).next_is_forward);
}

}
