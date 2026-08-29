#include "mod/examples/ExampleFactory.h"

#include "mod/MyMod.h"

#include "mod/examples/ex01_hello_connection.h"
#include "mod/examples/ex02_script.h"
#include "mod/examples/ex03_communication.h"
#include "mod/examples/ex04_calendar.h"

#include <string>

namespace my_mod {
namespace examples {

namespace {

// Minimal, forgiving extraction of {"example":"XX"} from a JSON text.
[[nodiscard]] std::string extractExampleId(std::string const& configJson) {
    auto pos = configJson.find("\"example\"");
    if (pos == std::string::npos) return {};
    auto colon = configJson.find(':', pos);
    if (colon == std::string::npos) return {};
    auto q1 = configJson.find('"', colon);
    if (q1 == std::string::npos) return {};
    auto q2 = configJson.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return configJson.substr(q1 + 1, q2 - q1 - 1);
}

} // namespace

std::unique_ptr<examples::ExampleBase> ExampleFactory::create(
    dearoreui::api::IDearOreUIApi& api,
    MyMod&                         mod,
    std::string                    configJson
) {
    auto id = extractExampleId(configJson);
    if (id == "01") return std::make_unique<Ex01HelloConnection>(api, mod);
    if (id == "02") return std::make_unique<Ex02Script>(api, mod);
    if (id == "03") return std::make_unique<Ex03Communication>(api, mod);
    // Default: 04 - Calendar Demo
    return std::make_unique<Ex04Calendar>(api, mod);
}

} // namespace examples
} // namespace my_mod