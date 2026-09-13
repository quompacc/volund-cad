#pragma once
#include "CadViewportScene.hpp"
#include "NativeOcctWindow.hpp"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

namespace voelund::viewport {
class CadViewport::Impl final : public CadViewportScene {
  public:
    void initialize(std::uintptr_t nativeWindowId) {
        if (!view.IsNull())
            return;
        displayConnection = new Aspect_DisplayConnection();
        graphicDriver = new OpenGl_GraphicDriver(displayConnection);
        configureScene();
        window = platform::makeOcctWindow(displayConnection, nativeWindowId);
        view->SetWindow(window);
        if (!window->IsMapped())
            window->Map();
        ready = true;
        displayPendingBodies();
    }
    void resize() {
        if (view.IsNull() || window.IsNull())
            return;
        window->DoResize();
        view->MustBeResized();
        view->Invalidate();
    }
    ~Impl() {
        if (!view.IsNull())
            view->Remove();
    }
    occ::handle<Aspect_DisplayConnection> displayConnection;
    occ::handle<Aspect_Window> window;
};
} // namespace voelund::viewport
