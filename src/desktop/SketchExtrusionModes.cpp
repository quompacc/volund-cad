#include "SketchExtrusionToolController.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>

// M23/M24: the sweep and loft modes of the extrusion panel. A sweep follows all drawn
// elements of an earlier sketch; a loft runs from the one picked region to the first region of
// another sketch, ruled or smooth.
namespace voelund::desktop {

void SketchExtrusionToolController::beginSelectedSweep() {
    if (!session_.canExtrudeSelectedSketchProfiles())
        return;
    editing_.reset();
    stored_.reset();
    const auto selected = session_.selectedSketchProfiles();
    profileSketch_ = *session_.selectedFeature();
    collectPaths();
    if (paths_.empty()) {
        statusChanged_(QObject::tr("Folgen braucht eine frühere Skizze als Pfad"));
        return;
    }
    document::ExtrusionParameters values{10.0, {}};
    values.sweep = paths_.back().second; // the latest sketch before this one
    open(*profileSketch_, {selected.begin(), selected.end()}, values);
}

void SketchExtrusionToolController::collectPaths() {
    paths_.clear();
    for (const auto& feature : session_.features()) {
        if (feature.id == *profileSketch_)
            break;
        if (feature.kind != document::FeatureKind::sketch)
            continue;
        document::SweepPath path{feature.id, {}};
        for (const auto& element : session_.sketchParameters(feature.id).elements.values)
            if (!element.construction)
                path.elements.push_back(element.id);
        if (!path.elements.empty())
            paths_.push_back({QString::fromStdString(feature.name), std::move(path)});
    }
}

void SketchExtrusionToolController::beginSelectedLoft() {
    if (!session_.canExtrudeSelectedSketchProfiles())
        return;
    const auto selected = session_.selectedSketchProfiles();
    if (selected.size() != 1) {
        statusChanged_(QObject::tr("Ausformung beginnt mit genau einem Skizzenbereich"));
        return;
    }
    editing_.reset();
    stored_.reset();
    profileSketch_ = *session_.selectedFeature();
    collectSections();
    if (sections_.empty()) {
        statusChanged_(QObject::tr("Ausformung braucht eine weitere Skizze mit einem Bereich"));
        return;
    }
    document::ExtrusionParameters values{10.0, {}};
    values.loft = document::LoftParameters{{sections_.back().second}, true};
    open(*profileSketch_, {selected.begin(), selected.end()}, values);
}

void SketchExtrusionToolController::collectSections() {
    sections_.clear();
    for (const auto& feature : session_.features()) {
        if (feature.id == *profileSketch_ || feature.kind != document::FeatureKind::sketch)
            continue;
        const auto regions = features::identifySketchRegions(
                                 session_.sketchParameters(feature.id).elements.values)
                                 .ids;
        if (!regions.empty())
            sections_.push_back(
                {QString::fromStdString(feature.name), {feature.id, regions.front()}});
    }
}

} // namespace voelund::desktop
