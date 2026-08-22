#pragma once

#include "mod/examples/ExampleBase.h"

#include <memory>
#include <string>

namespace my_mod {
namespace examples {

/// Builds the example selected by config.json (["example"]), default "07".
/// Falls back to the default example and logs a warning for unknown ids.
/// Example 06 (host method / single JS->C++ dispatch) is kept but disabled
/// by default: it can only be selected explicitly.
class ExampleFactory {
public:
    /// @param configPath Directory containing config.json (mod dir).
    static std::unique_ptr<examples::ExampleBase> create(
        dearoreui::api::IDearOreUIApi& api,
        MyMod&                         mod,
        std::string                    configJson
    );
};

} // namespace examples
} // namespace my_mod