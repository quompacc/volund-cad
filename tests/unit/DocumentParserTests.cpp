#include "../../src/document/BoundedDocumentInput.hpp"
#include "../../src/document/DocumentSerializationDetail.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/ModelDocument.hpp>

namespace {
using namespace voelund::document;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string fixture(const std::filesystem::path& root, int version) {
    std::ostringstream name;
    name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
    std::ifstream file(root / name.str(), std::ios::binary);
    require(file.is_open(), "Missing version fixture");
    return {std::istreambuf_iterator<char>(file), {}};
}
ModelDocument parse(const std::string& text) {
    std::istringstream input(text);
    return DocumentSerializer::read(input);
}
std::string serialize(const ModelDocument& document) {
    std::ostringstream output;
    DocumentSerializer::write(document, output);
    return output.str();
}
std::string changed(std::string text, const std::string& from, const std::string& to) {
    const auto index = text.find(from);
    require(index != std::string::npos, "Mutation does not match fixture");
    text.replace(index, from.size(), to);
    return text;
}
void rejects(const std::string& text, ModelDocument& current) {
    const auto before = DocumentSnapshot::capture(current);
    bool rejected = false;
    try {
        current = parse(text);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected && DocumentSnapshot::capture(current) == before,
            "Malformed input accepted or current model changed");
}
void migration(const ModelDocument& document, int version) {
    require(document.features().size() == 4 && !document.canUndo() && !document.canRedo(),
            "Feature count or load history incorrect");
    require(document.boxDimensions(FeatureId{1}).height == 30, "Box changed");
    const auto& cylinder = document.cylinderParameters(FeatureId{2});
    require(cylinder.radius == 2 && cylinder.placement.translationZ == 3 &&
                cylinder.resultMode == CylinderResultMode::separate,
            "Cylinder changed");
    const auto& sketch = document.sketchParameters(FeatureId{3});
    const std::size_t count = version == 1    ? 1
                              : version < 8   ? 3
                              : version < 15  ? 4
                              : version == 15 ? 5
                                              : 6;
    require(sketch.elements.values.size() == count && sketch.elements.nextId == count + 1,
            "Sketch migration lost element IDs");
    require(sketch.plane.originZ == 30 && sketch.plane.normalZ == 1, "Sketch plane changed");
    if (version == 1) {
        const auto& rectangle = std::get<SketchRectangle>(sketch.elements.values[0].geometry);
        require(rectangle.first == SketchPoint{1, 2} && rectangle.second == SketchPoint{6, 8},
                "Legacy rectangle migration incorrect");
    } else {
        const auto& line = std::get<SketchLine>(sketch.elements.values[0].geometry);
        require(line.constraint ==
                    (version == 2 ? SketchLineConstraint::none : SketchLineConstraint::horizontal),
                "Legacy direction default incorrect");
    }
    require(sketch.constraints.values.size() == (version < 3 ? 0U : 1U),
            "Constraint migration count");
    if (version >= 3) {
        const SketchConstraintKind kinds[] = {SketchConstraintKind::horizontal,
                                              SketchConstraintKind::horizontal,
                                              SketchConstraintKind::horizontal,
                                              SketchConstraintKind::coordinateX,
                                              SketchConstraintKind::equal,
                                              SketchConstraintKind::symmetric,
                                              SketchConstraintKind::pointDistance,
                                              SketchConstraintKind::pointDistanceX,
                                              SketchConstraintKind::midpoint,
                                              SketchConstraintKind::diameter,
                                              SketchConstraintKind::horizontalPoints,
                                              SketchConstraintKind::fixed,
                                              SketchConstraintKind::radius,
                                              SketchConstraintKind::coordinateX};
        const auto& constraint = sketch.constraints.values.front();
        require(constraint.kind == kinds[version - 3] && sketch.constraints.nextId == 2,
                "Constraint kind/next ID migration incorrect");
        require(constraint.third.has_value() == (version == 8),
                "Third-reference default incorrect");
        require(constraint.auxiliaryValues ==
                    (version == 14 ? std::vector<double>{20, 2} : std::vector<double>{}),
                "Auxiliary migration incorrect");
    }
    for (const auto& element : sketch.elements.values)
        require(element.construction == (version >= 10 && element.id.value() == 4),
                "Construction default incorrect");
    if (version >= 15)
        require(std::get<SketchArc>(sketch.elements.values[4].geometry).endAngle == 1.5,
                "Arc changed");
    if (version >= 16)
        require(std::get<SketchEllipse>(sketch.elements.values[5].geometry).minorRadius == 2,
                "Ellipse changed");
    const auto& extrusion = document.extrusionParameters(FeatureId{4});
    require(extrusion.distance == 5 &&
                extrusion.profiles == (version >= 5
                                           ? std::vector<SketchProfileId>{SketchProfileId{1}}
                                           : std::vector<SketchProfileId>{}),
            "Legacy all-profile default incorrect");
}
void versions(const std::filesystem::path& root) {
    for (int version = 1; version <= 16; ++version) {
        const auto text = fixture(root, version);
        const auto document = parse(text);
        migration(document, version);
        const auto canonical = serialize(document);
        require(canonical.starts_with("BINFILE") &&
                    DocumentSnapshot::capture(parse(canonical)) ==
                        DocumentSnapshot::capture(document) &&
                    parse(canonical).documentId() == document.documentId(),
                "Current-format roundtrip unstable");
        migration(parse(canonical), version);
        std::string crlf;
        for (char c : text) {
            if (c == '\n')
                crlf += '\r';
            crlf += c;
        }
        require(DocumentSnapshot::capture(parse(crlf)) == DocumentSnapshot::capture(document),
                "CRLF differs from LF");
        auto current = document;
        // Every missing required record must fail; a final LF itself is optional.
        for (std::size_t pos = 0; pos < text.size(); ++pos)
            if (pos == 0 || (text[pos - 1] == '\n' && pos < text.size()))
                rejects(text.substr(0, pos), current);
    }
}
void corruptions(const std::filesystem::path& root) {
    const auto source = fixture(root, 16);
    auto current = parse(source);
    const std::pair<std::string, std::string> mutations[] = {
        {"VOLUND 16", "VOLUND 17"},
        {"NEXT_ID 5", "NEXT_ID -1"},
        {"NEXT_ID 5", "NEXT_ID 4"},
        {"NEXT_ID 5", "NEXT_ID 18446744073709551615"},
        {"FEATURES 4", "FEATURES 10001"},
        {"BOX 1", "BOX -1"},
        {"CYLINDER 2", "CYLINDER 1"},
        {"\"Cylinder\" 1", "\"Cylinder\" 99"},
        {"\"Sketch\" 1", "\"Sketch\" 99"},
        {"\"Extrusion\" 3", "\"Extrusion\" 1"},
        {"20 20 30", "nan 20 30"},
        {"20 20 30", "1e999 20 30"},
        {"6 7 1 2", "1000001 7 1 2"},
        {"6 7 1 2", "6 7 2000001 2"},
        {"ELEMENT LINE 2", "ELEMENT LINE 1"},
        {"6 7 1 2", "6 6 1 2"},
        {"COORDINATE_X 6", "COORDINATE_X 99"},
        {"ELEMENT ELLIPSE 6 40 40 4 2", "ELEMENT ELLIPSE 6 40 40 1 2"},
        {"40 0 START 0", "40 0 START 5"},
        {"3 5 1 1", "3 5 2 1 1"},
        {"\"Support\"", "\"" + std::string(detail::maximumNameBytes + 1, 'a') + "\""}};
    for (const auto& [from, to] : mutations)
        rejects(changed(source, from, to), current);
    rejects(source + "unexpected\n", current);
    rejects(source + std::string(1, '\0'), current);
    const std::string constraint = "CONSTRAINT 1 COORDINATE_X 6 CENTER 0 START 40 0 START 0\n";
    rejects(changed(changed(source, "6 7 1 2", "6 7 2 3"), constraint, constraint + constraint),
            current);
    auto symmetric = fixture(root, 8);
    rejects(changed(symmetric, "0 4 START", "0 99 START"), current);
    rejects(changed(fixture(root, 6), "VOLUND 6", "VOLUND 4"), current);
    rejects(changed(source, "0 0 30 0 0 1 1 0 0", "0 0 30 1e308 0 0 1e308 0 0"), current);
    rejects(changed(source, "6 7 1 2", "6 18446744073709551615 1 2"), current);
    require(parse(changed(source, "\"Support\"",
                          "\"" + std::string(detail::maximumNameBytes, 'x') + "\""))
                    .features()
                    .front()
                    .name.size() == detail::maximumNameBytes,
            "Exact name limit rejected");
}

// Produces oversized whitespace without allocating it; tracks exactly how far the parser reads.
class PaddingBuffer final : public std::streambuf {
  public:
    PaddingBuffer(std::size_t size, std::size_t newlineEvery)
        : remaining_(size), interval_(newlineEvery) {}
    std::size_t consumed{};

  protected:
    int_type underflow() override {
        if (gptr() && gptr() < egptr())
            return traits_type::to_int_type(*gptr());
        if (prefixPosition_ < prefix_.size()) {
            byte_ = prefix_[prefixPosition_++];
            setg(&byte_, &byte_, &byte_ + 1);
            return traits_type::to_int_type(byte_);
        }
        if (remaining_ == 0)
            return traits_type::eof();
        --remaining_;
        ++consumed;
        byte_ = interval_ != 0 && consumed % interval_ == 0 ? '\n' : ' ';
        setg(&byte_, &byte_, &byte_ + 1);
        return traits_type::to_int_type(byte_);
    }

  private:
    std::size_t remaining_, interval_;
    const std::string prefix_{"VOLUND 16\nNEXT_ID 1\nFEATURES 0\n"};
    std::size_t prefixPosition_{};
    char byte_{};
};
void resourceLimits() {
    for (const auto& [size, newlineEvery] :
         {std::pair{detail::maximumLineBytes + 100, std::size_t{0}},
          std::pair{detail::maximumDocumentBytes + 100, std::size_t{1024}},
          std::pair{detail::maximumDocumentRecords + 100, std::size_t{1}}}) {
        PaddingBuffer buffer(size, newlineEvery);
        std::istream input(&buffer);
        bool rejected = false;
        try {
            static_cast<void>(DocumentSerializer::read(input));
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("limit") != std::string::npos;
        }
        require(rejected && (size <= detail::maximumDocumentBytes || buffer.consumed < size),
                "Resource limit consumed entire oversized source");
    }
    std::istringstream failed("VOLUND 16\nNEXT_ID 1\nFEATURES 0\n");
    failed.setstate(std::ios::badbit);
    bool rejected = false;
    try {
        static_cast<void>(DocumentSerializer::read(failed));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "I/O failure accepted as EOF");
    std::ostringstream chain;
    chain << "VOLUND 16\nNEXT_ID 258\nFEATURES 257\nBOX 1 \"Root\" 1 1 1 0 0 0 0 0 0\n";
    for (int id = 2; id <= 257; ++id)
        chain << "CYLINDER " << id << " \"Child\" " << id - 1 << " 1 1 0 0 0 0 0 0 JOIN\n";
    auto current = ModelDocument::empty();
    rejects(chain.str(), current);
    std::istringstream exceptionStream("VOLUND 16\nNEXT_ID 1\nFEATURES 0\n");
    exceptionStream.exceptions(std::ios::badbit | std::ios::failbit);
    require(DocumentSerializer::read(exceptionStream).features().empty(),
            "Exception-enabled EOF rejected");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass fixture directory");
        versions(argv[1]);
        corruptions(argv[1]);
        resourceLimits();
        std::cout << "PASS 16 version fixtures, semantic migration, roundtrip/CRLF, truncation, "
                     "corrupt references/counts/values, byte/line/record limits and I/O failure\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
