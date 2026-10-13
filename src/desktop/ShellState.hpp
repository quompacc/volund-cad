#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <QString>

#include <filesystem>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

// U02: read-only projection of everything the shell displays (ADR 0012). Widgets read it
// and never decide availability from their own state. Ownership: docs/desktop-shell.md.

// What the user has picked, from the most specific pick to the plain feature selection.
enum class ShellSelection {
    nothing,
    basePlane,
    planarFace,
    body, // M01: a whole body, picked by its row in the model tree
    sketchRegion,
    sketchElement,
    sketch,
    feature,
};

enum class ShellTool { none, sketchEditor, extrusion, transform };

// Picks that live only in the view (not in the document) and the active tool. MainWindow
// owns these; everything else comes from the CadSession.
struct ShellInputs final {
    std::optional<document::DatumPlane> basePlane;
    bool planarFace{false};
    ShellTool tool{ShellTool::none};
    // M01: a body picked by its tree row, with its shown name and visibility.
    std::optional<document::BodyId> body{};
    QString bodyName{};
    bool bodyHidden{false};
};

struct ShellState final {
    QString documentName;
    bool modified{false};
    bool canUndo{false};
    bool canRedo{false};
    bool modelCurrent{false};
    ShellSelection selection{ShellSelection::nothing};
    std::optional<document::FeatureId> feature;
    std::optional<document::FeatureKind> featureKind;
    std::optional<document::DatumPlane> basePlane;
    std::optional<document::BodyId> body;
    QString bodyName;
    bool bodyHidden{false};
    bool regionExtrudable{false};
    // M04: the selected feature is a sketch extrusion its panel can reopen.
    bool extrusionEditable{false};
    // M06-M08: the selected feature is a fillet, chamfer or hole its panel can reopen.
    bool filletEditable{false};
    ShellTool tool{ShellTool::none};

    // An open sketch or extrusion owns the document until OK, Cancel or Escape (K04, U05):
    // other tools, primitives and document undo wait.
    [[nodiscard]] bool toolBlocksCommands() const noexcept;
    // Command availability derived only from this state (U01 command table).
    [[nodiscard]] bool canSketch() const noexcept;
    [[nodiscard]] bool canExtrude() const noexcept;
    [[nodiscard]] bool canEditSketch() const noexcept;
    // M04 (user decision 2026-10-12): "Bearbeiten" reopens a sketch or a sketch extrusion.
    [[nodiscard]] bool canEditFeature() const noexcept;
    [[nodiscard]] QString windowTitle() const;
};

[[nodiscard]] QString nativeDisplayPath(const std::filesystem::path& path);
[[nodiscard]] QString documentDisplayName(const application::CadSession& session);
[[nodiscard]] ShellState projectShell(const application::CadSession& session,
                                      const ShellInputs& inputs);

} // namespace voelund::desktop
