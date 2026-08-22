#include "mod/examples/ExampleFactory.h"

#include "mod/MyMod.h"

#include "mod/examples/ex01_hello_connection.h"
#include "mod/examples/ex02_static_component.h"
#include "mod/examples/ex03_page_script.h"
#include "mod/examples/ex04_events.h"
#include "mod/examples/ex05_frame_data.h"
#include "mod/examples/ex06_host_method.h"
#include "mod/examples/ex07_calendar.h"

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
    if (id == "02") return std::make_unique<Ex02StaticComponent>(api, mod);
    if (id == "03") return std::make_unique<Ex03PageScript>(api, mod);
    if (id == "04") return std::make_unique<Ex04Events>(api, mod);
    if (id == "05") return std::make_unique<Ex05FrameData>(api, mod);
    // 06 is a capability demonstration kept OFF the default path: it can only
    // be selected explicitly (facet single dispatch; see its header).
    if (id == "06") return std::make_unique<Ex06HostMethod>(api, mod);
    if (id == "07" || id.empty()) {
        return std::make_unique<CalendarExample>(api, mod);
    }
    mod.getSelf().getLogger().warn("[example] unknown example id '{}', falling back to 07-calendar", id);
    return std::make_unique<CalendarExample>(api, mod);
}

} // namespace examples
} // namespace my_mod