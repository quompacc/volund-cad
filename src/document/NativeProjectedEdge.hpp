#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/EdgeReference.hpp>
namespace voelund::document::detail {
void writeNativeProjectedEdge(const TDF_Label& label, const ProjectedEdgeReference& reference);
ProjectedEdgeReference readNativeProjectedEdge(const TDF_Label& label);
} // namespace voelund::document::detail
