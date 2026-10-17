#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <TopoDS_Shape.hxx>

namespace voelund::features {

[[nodiscard]] document::SketchPlane resolveSketchPlane(const document::SketchPlane& previous,
                                                       const TopoDS_Shape& supportShape);

[[nodiscard]] bool updateSketchSupportReferences(document::SketchParameters& sketch,
                                                 const TopoDS_Shape& supportShape);

} // namespace voelund::features
