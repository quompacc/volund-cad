#include "SketchBoxSelection.hpp"

#include <algorithm>

namespace voelund::viewport {

std::vector<SketchSelectionIntent>
boxSelectionIntents(std::span<const SceneSketchElement> elements,
                    const occ::handle<V3d_View>& view, const QRect& rectangle) {
    std::vector<SketchSelectionIntent> result;
    if (view.IsNull())
        return result;
    for (const SceneSketchElement& element : elements) {
        const auto point = std::ranges::find_if(element.points, [&](const SceneSketchPoint& item) {
            int x = 0;
            int y = 0;
            view->Convert(item.worldX, item.worldY, item.worldZ, x, y);
            return rectangle.contains(x, y);
        });
        if (point != element.points.end())
            result.push_back({element.id, point->localX, point->localY, !result.empty()});
    }
    return result;
}

} // namespace voelund::viewport
