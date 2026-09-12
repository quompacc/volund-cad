#include "SketchViewAdapter.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

namespace voelund::desktop {

std::vector<viewport::SceneSketchElement> makeSceneSketchElements(
    const application::CadSession& session)
{
    std::vector<viewport::SceneSketchElement> result;
    const auto selected = session.selectedFeature();
    if (!selected.has_value()
        || session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch) {
        return result;
    }
    const auto evaluated = session.selectedSketchElements();
    result.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        result.push_back(viewport::SceneSketchElement{element.id.value(), element.shape});
    }
    return result;
}

QString sketchSelectionSummary(const application::CadSession& session)
{
    const auto& sketch = session.selectedSketchParameters();
    const auto selectedId = session.selectedSketchElement();
    QString summary = QObject::tr("Sketch — %1 elements")
        .arg(sketch.elements.values.size());
    if (!selectedId.has_value()) {
        return summary;
    }
    for (const document::SketchElement& element : sketch.elements.values) {
        if (element.id != *selectedId) {
            continue;
        }
        QString kind = QObject::tr("Circle");
        if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            kind = QObject::tr("Line");
        } else if (std::holds_alternative<document::SketchRectangle>(element.geometry)) {
            kind = QObject::tr("Rectangle");
        }
        return summary + QObject::tr("\n\nSelected: %1 #%2")
            .arg(kind)
            .arg(element.id.value());
    }
    return summary;
}

} // namespace voelund::desktop
