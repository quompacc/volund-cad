#include "SketchViewOrientation.hpp"

#include <gp_Vec.hxx>

namespace voelund::viewport {

void orientSketchView(const PlanarFaceSelection& support, const occ::handle<V3d_View>& view) {
    if (view.IsNull())
        return;
    const gp_Vec normal(support.normalX, support.normalY, support.normalZ);
    const gp_Vec xDirection(support.xDirectionX, support.xDirectionY, support.xDirectionZ);
    const gp_Vec up = normal.Crossed(xDirection);
    view->SetProj(normal.X(), normal.Y(), normal.Z());
    view->SetUp(up.X(), up.Y(), up.Z());
    view->FitAll(0.08, false);
    view->ZFitAll();
}

} // namespace voelund::viewport
