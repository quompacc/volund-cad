#include "SketchViewOrientation.hpp"

#include <gp_Dir.hxx>

namespace voelund::viewport {

void orientSketchView(const PlanarFaceSelection& support, const occ::handle<V3d_View>& view) {
    if (view.IsNull())
        return;
    // S03: the sketch frame decides the screen axes, x to the right and y upwards, so a
    // horizontal sketch line is horizontal on screen on every plane and support face.
    const gp_Dir normal(support.normalX, support.normalY, support.normalZ);
    const gp_Dir xDirection(support.xDirectionX, support.xDirectionY, support.xDirectionZ);
    const gp_Dir up = normal.Crossed(xDirection);
    view->SetProj(normal.X(), normal.Y(), normal.Z());
    view->SetUp(up.X(), up.Y(), up.Z());
    view->FitAll(0.08, false);
    view->ZFitAll();
}

} // namespace voelund::viewport
