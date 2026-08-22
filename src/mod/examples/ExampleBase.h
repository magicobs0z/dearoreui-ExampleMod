#pragma once

#include "api/IDearOreUIApi.h"
#include "api/types/Id.h"

#include <string_view>

namespace my_mod {

class MyMod;

namespace examples {

// Base class for every tutorial example. One example is active at a time,
// selected by config.json ("example": "07" by default / "01".."07").
// Each example owns its own ModId namespace and registration stack, so
// switching examples never leaks registrations across sessions.
class ExampleBase {
public:
    ExampleBase(dearoreui::api::IDearOreUIApi& api, MyMod& mod) : mApi(api), mMod(mod) {}
    virtual ~ExampleBase() = default;

    ExampleBase(ExampleBase const&)            = delete;
    ExampleBase& operator=(ExampleBase const&) = delete;

    /// Registers everything the example needs. Returns false and cleans up on
    /// failure. Called once on enable().
    [[nodiscard]] virtual bool registerAll() = 0;

    /// Reverses registration order. Idempotent; called once on disable().
    virtual void shutdown() = 0;

    /// Short label, e.g. "07-state-center" (used in logs).
    [[nodiscard]] virtual std::string_view name() const = 0;

protected:
    dearoreui::api::IDearOreUIApi& mApi;
    MyMod&                         mMod;
};

} // namespace examples
} // namespace my_mod