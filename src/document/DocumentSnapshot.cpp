#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
ModelDocument DocumentSnapshot::toDocument() const {
    auto document = ModelDocument::empty();
    document.features_ = features_;
    document.parameters_ = parameters_;
    document.nextFeatureId_ = nextFeatureId_;
    return document;
}
} // namespace voelund::document
