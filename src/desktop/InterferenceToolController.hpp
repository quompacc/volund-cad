// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <TopoDS_Shape.hxx>

#include <functional>
#include <vector>

namespace voelund::desktop {

class ToolPanelHost;

// N08 (agent decision 2026-10-27, as Fusion's interference check): "Überschneidungen" checks
// the selected bodies against each other, or all shown bodies when fewer than two are
// selected. The panel lists each overlapping pair with its common volume; the overlaps are
// outlined in red. Bodies that only touch do not count. It changes nothing; OK or Cancel ends
// it.
class InterferenceToolController final {
  public:
    struct Body final {
        QString name;
        TopoDS_Shape shape;
    };
    InterferenceToolController(viewport::CadViewport& viewport, ToolPanelHost& panel,
                               std::function<void(const QString&)> statusChanged);

    void begin(std::vector<Body> bodies, bool selected);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    // The shown report; full precision for the clipboard.
    [[nodiscard]] QString report(bool fullPrecision = false) const;
    [[nodiscard]] std::size_t overlaps() const noexcept { return found_.size(); }

  private:
    struct Overlap final {
        QString first;
        QString second;
        double volume;
    };
    void finish();

    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void(const QString&)> statusChanged_;
    std::vector<Overlap> found_;
    std::size_t checked_{0};
    bool selected_{false};
    bool active_{false};
};

} // namespace voelund::desktop
