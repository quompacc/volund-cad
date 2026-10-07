#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/ModelDocument.hpp>
namespace voelund::document::detail {
// Children 2-4 of `label`: origin feature, primitive role, extrusion boundary. Reading
// consumes the boundary from `remainingElements`. Role -2 marks the end cap of a face
// extrusion (T04, schema 10).
void writeNativeSubshapeOrigin(const TDF_Label& label, const SubshapeOrigin& origin);
SubshapeOrigin readNativeSubshapeOrigin(const TDF_Label& label, std::size_t& remainingElements,
                                        int schemaVersion);
void writeNativeSketchSupport(const TDF_Label&, const SketchSupportBinding&);
SketchSupportBinding readNativeSketchSupport(const TDF_Label&, std::size_t remainingElements,
                                             int schemaVersion);
} // namespace voelund::document::detail
