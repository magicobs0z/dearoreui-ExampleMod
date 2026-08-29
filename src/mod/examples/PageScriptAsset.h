#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace my_mod {
namespace examples {

// T1: reads a page-script asset shipped next to the mod (scripts/<filename>).
// Page scripts are independent .js files (assets/scripts/ in this repo, copied
// into the mod output by the build) instead of inline R"js()js" constants, so
// they can be linted / reused / mapped like normal frontend assets. The engine
// cannot execute DOM <script src> nodes, so the content is loaded here on the
// C++ side and pushed through the verified <script> DomNode injection channel.
[[nodiscard]] inline std::string loadPageScriptAsset(std::filesystem::path const& modDir, char const* filename) {
    std::ifstream stream(modDir / "scripts" / filename);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

} // namespace examples
} // namespace my_mod
