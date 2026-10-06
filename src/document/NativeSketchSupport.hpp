#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>
namespace voelund::document::detail {
// Children 2-4 of `label`: origin feature, primitive role, extrusion boundary. Reading
// consumes the boundary from `remainingElements`.
void writeNativeSubshapeOrigin(const TDF_Label& label, const SubshapeOrigin& origin);
SubshapeOrigin readNativeSubshapeOrigin(const TDF_Label& label, std::size_t& remainingElements);
void writeNativeSketchSupport(const TDF_Label&, const SketchSupportBinding&);
SketchSupportBinding readNativeSketchSupport(const TDF_Label&, std::size_t remainingElements);
} // namespace voelund::document::detail
