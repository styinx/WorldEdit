#pragma once

#include "msh/flat_model.hpp"
#include "odf/definition.hpp"

#include <filesystem>
#include <string_view>

namespace sk::assets {

template<typename T>
struct asset_traits {
};

template<>
struct asset_traits<odf::definition> {
   static constexpr std::string_view error_type_name = "object definition";

   static auto load(const std::filesystem::path& path) -> odf::definition;
};

template<>
struct asset_traits<msh::flat_model> {
   static constexpr std::string_view error_type_name = "model";

   static auto load(const std::filesystem::path& path) -> msh::flat_model;
};

}