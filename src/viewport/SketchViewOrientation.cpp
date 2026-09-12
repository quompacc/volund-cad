#include "SketchViewOrientation.hpp"

#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

#include <cmath>

namespace voelund::viewport {

void orientSketchView(const PlanarFaceSelection& support, const occ::handle<V3d_View>& view) {
    if (view.IsNull())
        return;
    const gp_Dir normal(support.normalX, support.normalY, support.normalZ);
    const gp_Vec preferredUp = std::abs(normal.Z()) < 0.95 ? gp_Vec(0.0, 0.0, 1.0)
                                                           : gp_Vec(0.0, 1.0, 0.0);
    const gp_Vec up = preferredUp - gp_Vec(normal) * preferredUp.Dot(gp_Vec(normal));
    view->SetProj(normal.X(), normal.Y(), normal.Z());
    view->SetUp(up.X(), up.Y(), up.Z());
    view->FitAll(0.08, false);
    view->ZFitAll();
}

} // namespace voelund::viewport
