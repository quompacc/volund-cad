#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>

namespace {
using namespace voelund::document;
static_assert(!std::is_convertible_v<FeatureId, BodyId>);
static_assert(!std::is_convertible_v<BodyId, FeatureId>);
static_assert(!std::is_convertible_v<BodyId, DocumentId>);
static_assert(DocumentId{1, 0} != DocumentId{0, 1});

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid identity input accepted");
}
void fixtures(const std::filesystem::path& directory) {
    for (int version = 1; version <= 16; ++version) {
        std::ostringstream name;
        name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
        std::ifstream input(directory / name.str());
        const auto imported = DocumentSerializer::readWithReport(input);
        const auto& snapshot = imported.snapshot;
        const auto expected = legacyBodyIdentities(snapshot.features(), snapshot.parameters());
        require(expected.size() == 4 && expected[0].body == BodyId{1} &&
                    expected[2].body == std::nullopt && expected[3].body == BodyId{1},
                "Legacy support/extrusion chain lost its root body");
        std::ostringstream saved;
        DocumentSerializer::write(snapshot.toDocument(), saved);
        std::istringstream reopened(saved.str());
        const auto roundtrip = DocumentSerializer::readWithReport(reopened);
        require(expected == legacyBodyIdentities(roundtrip.snapshot.features(),
                                                roundtrip.snapshot.parameters()),
                "Legacy roundtrip changed body mapping");
        auto features = std::vector(snapshot.features().begin(), snapshot.features().end());
        auto parameters = std::vector(snapshot.parameters().begin(), snapshot.parameters().end());
        for (auto& feature : features)
            feature.name = "Same renamed display name";
        std::ranges::reverse(features);
        std::ranges::reverse(parameters);
        auto reversed = legacyBodyIdentities(features, parameters);
        std::ranges::reverse(reversed);
        require(expected == reversed, "Names or paired row order changed body mapping");
    }
}
void independentBodiesAndExtension() {
    std::vector<FeatureDescriptor> features{
        {FeatureId{10}, FeatureKind::box, "same", std::nullopt},
        {FeatureId{20}, FeatureKind::cylinder, "same", FeatureId{10}},
        {FeatureId{30}, FeatureKind::cylinder, "same", FeatureId{20}},
        {FeatureId{40}, FeatureKind::box, "same", std::nullopt}};
    CylinderParameters cylinder{};
    cylinder.resultMode = CylinderResultMode::separate;
    std::vector<FeatureParameters> parameters{BoxParameters{}, cylinder, cylinder, BoxParameters{}};
    std::get<CylinderParameters>(parameters[2]).resultMode = CylinderResultMode::cut;
    const auto mapping = legacyBodyIdentities(features, parameters);
    require(mapping[0].body == BodyId{10} && mapping[1].body == BodyId{20} &&
                mapping[2].body == BodyId{20} && mapping[3].body == BodyId{40},
            "Separate body or modifying feature confused identity with producer");
    for (auto mode : {CylinderResultMode::join, CylinderResultMode::intersect}) {
        cylinder.resultMode = mode;
        const auto id = FeatureId{50 + static_cast<std::uint64_t>(features.size())};
        features.push_back({id, FeatureKind::cylinder, "same", FeatureId{30}});
        parameters.emplace_back(cylinder);
        require(legacyBodyIdentities(features, parameters).back().body == BodyId{20},
                "Additional operation replaced body identity");
    }
    std::ranges::reverse(features);
    std::ranges::reverse(parameters);
    auto reordered = legacyBodyIdentities(features, parameters);
    std::ranges::reverse(reordered);
    require(std::equal(mapping.begin(), mapping.end(), reordered.begin()),
            "Reorder merged independent bodies");
}
void invalidGraphs() {
    std::vector<FeatureDescriptor> features{
        {FeatureId{1}, FeatureKind::box, "Box", std::nullopt},
        {FeatureId{2}, FeatureKind::cylinder, "Join", FeatureId{1}}};
    std::vector<FeatureParameters> parameters{BoxParameters{}, CylinderParameters{}};
    const auto original = features;
    auto rejects = [&] { rejected([&] { (void)legacyBodyIdentities(features, parameters); }); };
    for (auto invalid : {std::uint64_t{0}, std::numeric_limits<std::uint64_t>::max() - 1,
                         std::numeric_limits<std::uint64_t>::max()}) {
        features[0].id = FeatureId{invalid};
        rejects();
        rejected([&] { (void)BodyId{invalid}; });
    }
    features = original;
    features[1].id = FeatureId{1}; rejects();
    features = original;
    features[1].inputFeature = FeatureId{99}; rejects();
    features[1].inputFeature = FeatureId{2}; rejects();
    features = original;
    features[0].kind = FeatureKind::cylinder;
    features[0].inputFeature = FeatureId{2};
    parameters[0] = CylinderParameters{}; rejects(); // two-node cycle
    features = original;
    rejects(); // parameter kind mismatch
    parameters[0] = BoxParameters{};
    features[0].inputFeature = FeatureId{2}; rejects();
    features = original;
    features[1].inputFeature.reset(); rejects();
    features = original;
    parameters.pop_back(); rejects();
    rejected([] { (void)DocumentId{0, 0}; });
    require(legacyBodyIdentities({}, {}).empty(), "Empty document gained a body");
}
void longChain() {
    constexpr std::size_t count = 20000;
    std::vector<FeatureDescriptor> features{{FeatureId{1}, FeatureKind::box, "", std::nullopt}};
    std::vector<FeatureParameters> parameters{BoxParameters{}};
    for (std::size_t index = 1; index < count; ++index) {
        features.push_back({FeatureId{index + 1}, FeatureKind::cylinder, "", FeatureId{index}});
        parameters.emplace_back(CylinderParameters{});
    }
    std::ranges::reverse(features);
    std::ranges::reverse(parameters);
    const auto result = legacyBodyIdentities(features, parameters);
    require(result.size() == count && std::ranges::all_of(result, [](const auto& item) {
        return item.body == BodyId{1};
    }), "Long reverse-ordered chain changed body identity");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Fixture directory required");
        fixtures(argv[1]);
        independentBodiesAndExtension();
        invalidGraphs();
        longChain();
        std::cout << "Legacy body identity migration passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
