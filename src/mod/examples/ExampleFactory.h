#pragma once

#include "mod/examples/ExampleBase.h"

#include <memory>
#include <string>

namespace my_mod {
namespace examples {

/// Builds the example selected by config.json (["example"]), default "04".
/// 01 - Hello Connection (bridge + mod registration)
/// 02 - Component Script (component tree + page script)
/// 03 - Communication (events + frame clock + host method)
/// 04 - Calendar Demo (polished full-featured calendar)
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