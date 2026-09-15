#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
DocumentSnapshot DocumentSnapshot::capture(const ModelDocument& document) {
    return {document.features_, document.parameters_, document.nextFeatureId_};
}
bool DocumentSnapshot::matchesContent(const ModelDocument& document) const {
    return features_ == document.features_ && parameters_ == document.parameters_;
}
ModelDocument DocumentSnapshot::toDocument() const {
    auto document = ModelDocument::empty();
    document.features_ = features_;
    document.parameters_ = parameters_;
    document.nextFeatureId_ = nextFeatureId_;
    return document;
}
} // namespace voelund::document
