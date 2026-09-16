#include "ModelDocumentImpl.hpp"
#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
DocumentSnapshot DocumentSnapshot::capture(const ModelDocument& document) {
    return {document.impl_->features_, document.impl_->parameters_, document.impl_->nextFeatureId_};
}
bool DocumentSnapshot::matchesContent(const ModelDocument& document) const {
    return features_ == document.impl_->features_ && parameters_ == document.impl_->parameters_;
}
ModelDocument DocumentSnapshot::toDocument() const {
    auto document = ModelDocument::empty();
    document.impl_->features_ = features_;
    document.impl_->parameters_ = parameters_;
    document.impl_->nextFeatureId_ = nextFeatureId_;
    return document;
}
} // namespace voelund::document
