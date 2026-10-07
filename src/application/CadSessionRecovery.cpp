#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/DocumentSnapshot.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>

// L05b: an open sketch draft goes to its own recovery file, apart from the confirmed document.
// The file is the document with the draft applied in the native format, so every element,
// constraint, plane and support binding survives exactly, also a draft that does not solve.
namespace voelund::application {

CadSession::SketchDraftSnapshot
CadSession::sketchDraftSnapshot(std::optional<document::FeatureId> edited,
                                std::optional<document::FeatureId> support,
                                const document::SketchParameters& draft) const {
    auto copy = document::DocumentSnapshot::capture(document_).toDocument(document_.documentId());
    document::FeatureId feature{0};
    if (edited) {
        if (!copy.setSketchParameters(*edited, draft))
            throw std::invalid_argument("The sketch draft does not fit the edited sketch");
        feature = *edited;
    } else if (draft.datum) {
        feature = copy.addDatumSketchFeature("Skizzenentwurf", draft);
    } else if (support) {
        feature = copy.addSketchFeature("Skizzenentwurf", *support, draft);
    } else {
        throw std::invalid_argument("A sketch draft needs a base plane or a support");
    }
    std::ostringstream output;
    document::DocumentSerializer::write(copy, output);
    return {output.str(), feature};
}

CadSession::SketchDraft CadSession::readSketchDraft(std::string_view bytes,
                                                    document::FeatureId feature) {
    std::istringstream input{std::string(bytes)};
    const auto document = document::DocumentSerializer::read(input);
    const auto features = document.features();
    const auto descriptor = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (descriptor == features.end() || descriptor->kind != document::FeatureKind::sketch)
        throw std::invalid_argument("The sketch draft is missing");
    return {document.sketchParameters(feature), descriptor->inputFeature};
}

} // namespace voelund::application
