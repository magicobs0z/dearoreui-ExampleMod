#pragma once

#include "mod/examples/ExampleBase.h"

#include <string_view>

namespace my_mod {
namespace examples {

// Example 01 - Hello Connection.
//
// Teaches the minimum integration contract:
//   1. A mod-allocated API instance is obtained through the pure C bridge
//      (DearOreUIBridge, see MyMod::connectDearOreUI) - no C++ link against
//      DearOreUI.lib required, only the header.
//   2. registerMod() announces the mod identity; every later registration
//      (UI, host method, subscription) validates its owner against it.
//   3. getInfo()/getProtocolVersion()/isReady() let the mod verify the
//      prerequisite mod is alive and the API surface is usable.
//
// There is no UI in this example: the "success marker" is the log output
// from the LE console / host log:
//   [example.hello] connected: protocol=v1 ready=true minecraft=... oreui=... coherent=...
class Ex01HelloConnection final : public ExampleBase {
public:
    Ex01HelloConnection(dearoreui::api::IDearOreUIApi& api, MyMod& mod)
    : ExampleBase(api, mod),
      mModId("example.hello") {}

    ~Ex01HelloConnection() override = default;

    [[nodiscard]] bool registerAll() override {
        auto& logger = mMod.getSelf().getLogger();

        dearoreui::api::ModManifest manifest;
        manifest.id           = mModId;
        manifest.modNamespace = mModId.value();
        manifest.displayName  = "Hello Connection Example";
        manifest.modVersion   = dearoreui::api::Version{1, 0, 0};
        // The simplest example needs no special permissions.
        manifest.permissions  = {};

        auto registered = mApi.registerMod(manifest);
        if (registered.isErr()) {
            logger.error("[example.hello] registerMod failed: {}", registered.error().message);
            return false;
        }

        auto info     = mApi.getInfo();
        auto protocol = mApi.getProtocolVersion();
        auto ready    = mApi.isReady();
        // ApiInfo carries the host runtime versions, not a display name.
        logger.info(
            "[example.hello] connected: protocol=v{} ready={} minecraft={} oreui={} coherent={}",
            protocol,
            ready,
            info.minecraftVersion,
            info.oreuiVersion,
            info.coherentVersion
        );
        return true;
    }

    void shutdown() override {
        // unregisterMod is idempotent and keyed by ModId.
        static_cast<void>(mApi.unregisterMod(mModId));
    }

    [[nodiscard]] std::string_view name() const override { return "01-hello-connection"; }

private:
    dearoreui::api::ModId mModId;
};

} // namespace examples
} // namespace my_mod