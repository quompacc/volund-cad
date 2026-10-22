// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Shape.hxx>

namespace voelund::document {
class ModelDocument;
class DocumentSnapshot;
} // namespace voelund::document

namespace voelund::features {

class BoxFeature final {
  public:
    explicit BoxFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] kernel::ShapeOperationResult
    evaluateWithHistory(const document::DocumentSnapshot& document) const;
    [[nodiscard]] TopoDS_Shape evaluate(const document::ModelDocument& document) const;
    [[nodiscard]] TopoDS_Shape evaluate(const document::DocumentSnapshot& document) const;

  private:
    document::FeatureId featureId_;
};

} // namespace voelund::features
