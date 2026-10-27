// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QString>
#include <QStringList>

#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <functional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M12 (user decision 2026-10-15): "Messen" measures without changing the document. A click on an
// edge or a planar face shows its length, radius or area and the volume of its body; a second
// click adds the distance and the angle between both; a third starts over. Values show with
// three decimals and their unit; "Werte kopieren" copies them in full. While the model is not
// current, measuring is locked. N08: corners can be clicked too, and an edge click can measure
// from the edge's middle or its circle's centre; two points also show the difference per axis.
class MeasureToolController final {
  public:
    MeasureToolController(application::CadSession& session, viewport::CadViewport& viewport,
                          ToolPanelHost& panel, std::function<void(const QString&)> statusChanged);

    void begin();
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void pickedEdge(std::uint64_t body, const TopoDS_Shape& edge);
    void pickedFace(const viewport::PlanarFaceSelection& face);
    void cancel();
    // The shown report, and the same in full precision.
    [[nodiscard]] QString report(bool fullPrecision = false) const;

  private:
    struct Pick final {
        std::uint64_t body;
        TopoDS_Shape shape;
    };
    void picked(Pick pick);
    [[nodiscard]] QStringList describe(std::size_t index, bool fullPrecision) const;
    [[nodiscard]] QStringList between(bool fullPrecision) const;
    void show();
    void finish();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void(const QString&)> statusChanged_;
    std::vector<Pick> picks_;
    bool active_{false};
    int edgeAs_{0}; // N08: 0 the edge, 1 its middle, 2 its circle's centre
};

} // namespace voelund::desktop
