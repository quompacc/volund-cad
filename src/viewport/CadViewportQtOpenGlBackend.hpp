#pragma once
#include "CadViewportScene.hpp"
#include "QtOcctGlContext.hpp"
#include <QPointer>

namespace voelund::viewport {
class CadViewport::Impl final : public CadViewportScene {
  public:
    Impl() = default;
    void createScene() {
        occ::handle<Aspect_DisplayConnection> connection;
        auto driver = new OpenGl_GraphicDriver(connection, false);
        driver->ChangeOptions().buffersNoSwap = true;
        driver->ChangeOptions().buffersOpaqueAlpha = true;
        driver->ChangeOptions().useSystemBuffer = false;
        graphicDriver = driver;
        configureScene();
    }
    QPointer<QOpenGLContext> boundContext;
    QMetaObject::Connection contextConnection;
    std::uint64_t contextGeneration{0};
    std::uint64_t cleanupGeneration{0};
    std::uint64_t renderedFrames{0};
    bool cleaningUp{false};
    bool failNextInitialization{false}; // Controlled by the friend test driver only.
    std::string lastError;
};
} // namespace voelund::viewport
