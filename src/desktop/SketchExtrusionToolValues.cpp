// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "MainWindowSupport.hpp"
#include "SketchExtrusionToolController.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>

namespace voelund::desktop {

// Distance, start offset and taper (W16); false for any other field.
bool SketchExtrusionToolController::setLength(const std::string& id, double value) {
    if (id == "distance")
        setDistance(value);
    else if (id == "offset")
        offset_ = value;
    else if (id == "taper")
        taper_ = value;
    else
        return false;
    return true;
}

// A base-plane sketch joins or cuts a named body; the field shows when there is a choice.
// The index of the stored target among them (0 when none).
int SketchExtrusionToolController::collectTargets(const std::optional<document::BodyId>& stored,
                                                  std::vector<QString>& names) {
    targets_.clear();
    const auto outline = session_.modelOutline();
    for (const auto& body : outline.bodies) {
        if (!baseSketch_ ||
            std::ranges::find(session_.validBodies(), body.id, &features::EvaluatedBody::bodyId) ==
                session_.validBodies().end())
            continue;
        targets_.push_back(body.id);
        names.push_back(bodyLabel(outline, body.id)); // M01: named as in the tree
    }
    target_ = targets_.empty() ? std::nullopt : std::optional{targets_.front()};
    const auto found = stored ? std::ranges::find(targets_, *stored) : targets_.end();
    if (found == targets_.end())
        return 0;
    target_ = *found;
    return static_cast<int>(found - targets_.begin());
}

} // namespace voelund::desktop
