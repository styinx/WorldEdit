
#include "asset_libraries.hpp"

#include <string_view>
#include <unordered_set>

using namespace std::literals;

namespace sk::assets {

namespace {

const std::unordered_set ignored_folders = {L"_BUILD"sv,    L"_LVL_PC"sv,
                                            L"_LVL_PS2"sv,  L"_LVL_PSP"sv,
                                            L"_LVL_XBOX"sv,

                                            L".git"sv,      L".svn"sv,
                                            L".vscode"sv};

}

libraries_manager::libraries_manager(output_stream& stream) noexcept
   : odfs{stream}, models{stream}
{
}

void libraries_manager::source_directory(const std::filesystem::path& source_directory) noexcept
{

   for (auto entry =
           std::filesystem::recursive_directory_iterator{
              source_directory,
              std::filesystem::directory_options::follow_directory_symlink |
                 std::filesystem::directory_options::skip_permission_denied};
        entry != std::filesystem::end(entry); ++entry) {
      const auto& path = entry->path();

      if (ignored_folders.contains((--path.end())->native())) {
         entry.disable_recursion_pending();

         continue;
      }

      if (auto extension = path.extension(); extension == L".odf"sv) {
         odfs.add_asset(path);
      }
      else if (extension == L".msh"sv) {
         models.add_asset(path);
      }
   }
}

}
