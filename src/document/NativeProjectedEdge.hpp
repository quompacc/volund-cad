// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/EdgeReference.hpp>
namespace voelund::document::detail {
// Children 2-5 (primitive) or 2-7 (contour) of `label` hold the edge origin.
void writeNativeEdgeOrigin(const TDF_Label& label, const EdgeOrigin& origin);
EdgeOrigin readNativeEdgeOrigin(const TDF_Label& label);
void writeNativeProjectedEdge(const TDF_Label& label, const ProjectedEdgeReference& reference);
ProjectedEdgeReference readNativeProjectedEdge(const TDF_Label& label);
} // namespace voelund::document::detail
