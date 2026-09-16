#include "../../src/document/NativeContainerStructure.hpp"
#include "../../src/document/NativeContainerHeader.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include "../../src/document/DocumentSerializationDetail.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void put(std::string& bytes, std::size_t at, std::uint64_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i)
        bytes.at(at + i) = static_cast<char>((value >> (i * 8)) & 255);
}
void append(std::string& bytes, std::uint64_t value, unsigned width = 4) {
    const auto at = bytes.size();
    bytes.resize(at + width);
    put(bytes, at, value, width);
}
void rejected(const std::string& bytes) {
    bool failed = false;
    try { validateNativeContainerStructure(bytes); }
    catch (const std::invalid_argument&) { failed = true; }
    require(failed, "Invalid native structure accepted");
}
void mutate(const std::string& bytes, std::size_t at, std::uint64_t value, unsigned width = 4) {
    auto changed = bytes;
    put(changed, at, value, width);
    rejected(changed);
}
std::string label(std::uint32_t tag, const std::string& attributes, const std::string& children) {
    std::string bytes;
    append(bytes, tag);
    append(bytes, 16 + attributes.size() + children.size(), 8);
    bytes += attributes;
    append(bytes, 0xffffffff);
    bytes += children;
    append(bytes, 0xfffffffe);
    return bytes;
}
void framing(const std::string& valid) {
    const auto payload = validateNativeContainerHeader(valid);
    const auto prefix = valid.substr(0, payload + 32); // validated metadata + end-section TOC
    const auto root = label(0, {}, {});
    validateNativeContainerStructure(prefix + root);
    for (std::size_t n = payload; n < valid.size(); ++n) rejected(valid.substr(0, n));
    rejected(valid + "extra");
    rejected(valid + valid);
    mutate(valid, payload, 512); // section name size must not reach OCCT's fixed buffer
    mutate(valid, payload + 4, 0); // section name
    for (const auto offset : {8U, 16U, 24U}) mutate(valid, payload + offset, UINT64_MAX, 8);
    mutate(valid, payload + 32, 1); // root tag
    mutate(valid, payload + 36, UINT64_MAX, 8); // root length
    mutate(valid, payload + 36, 8, 8); // too short for markers
    auto synthetic = prefix + label(0, {}, label(1, {}, {}));
    mutate(synthetic, payload + 36, synthetic.size() - (payload + 36) - 1, 8);
    mutate(synthetic, payload + 52, UINT64_MAX, 8); // child's size escapes its parent
    mutate(synthetic, synthetic.size() - 4, 0xffffffff); // wrong end-label marker
    mutate(synthetic, payload + 44, 0xfffffffe); // wrong end-attribute marker
    std::string attribute;
    append(attribute, 1); append(attribute, 1); append(attribute, 4); append(attribute, 42);
    synthetic = prefix + label(0, attribute, {});
    validateNativeContainerStructure(synthetic);
    mutate(synthetic, payload + 44, 0); // invalid type
    mutate(synthetic, payload + 48, 0); // invalid ID
    mutate(synthetic, payload + 48, 0xffffffff); // direct stream marker is unsupported
    mutate(synthetic, payload + 52, UINT32_MAX); // record length escapes label
    // This layer validates framing, not the semantics of the attribute driver payload.
    put(synthetic, payload + 56, UINT32_MAX);
    validateNativeContainerStructure(synthetic);
}
void limits(const std::string& valid) {
    const auto prefix = valid.substr(0, validateNativeContainerHeader(valid) + 32);
    std::string subtree;
    for (std::size_t i = 1; i < maximumFeatureDepth; ++i) subtree = label(1, {}, subtree);
    validateNativeContainerStructure(prefix + label(0, {}, subtree));
    rejected(prefix + label(0, {}, label(1, {}, subtree)));
    std::string siblings;
    siblings.reserve(maximumDocumentRecords * 20);
    for (std::size_t i = 1; i < maximumDocumentRecords; ++i)
        siblings += label(static_cast<std::uint32_t>(i), {}, {});
    validateNativeContainerStructure(prefix + label(0, {}, siblings));
    siblings += label(static_cast<std::uint32_t>(maximumDocumentRecords), {}, {});
    rejected(prefix + label(0, {}, siblings));
    std::string attributes;
    attributes.reserve((maximumDocumentRecords + 1) * 12);
    for (std::size_t i = 1; i <= maximumDocumentRecords; ++i) {
        append(attributes, 1); append(attributes, i); append(attributes, 0);
    }
    validateNativeContainerStructure(prefix + label(0, attributes, {}));
    append(attributes, 1); append(attributes, maximumDocumentRecords + 1); append(attributes, 0);
    rejected(prefix + label(0, attributes, {}));
}
void regressions() {
    auto model = ModelDocument::empty();
    (void)model.addBoxFeature("Structure Ω", {2, 3, 4});
    const auto snapshot = IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
    const auto bytes = NativeDocumentStore::encode(snapshot);
    validateNativeContainerStructure(bytes);
    require(NativeDocumentStore::decode(bytes) == snapshot, "Native roundtrip changed fields");
    framing(bytes);
    limits(bytes);
    bool failed = false;
    try { (void)NativeDocumentStore::decode(bytes + "extra"); }
    catch (const std::invalid_argument& error) {
        failed = std::string(error.what()).find("container structure") != std::string::npos;
    }
    require(failed, "Store did not reject trailing data before OCCT Open");
}
} // namespace
int main(int argc, char** argv) {
    try {
        regressions();
        for (int i = 1; i < argc; ++i) {
            std::ifstream input(argv[i], std::ios::binary);
            require(input.good(), "Missing exchange fixture");
            const std::string bytes{std::istreambuf_iterator<char>(input), {}};
            validateNativeContainerStructure(bytes);
            (void)NativeDocumentStore::decode(bytes);
        }
        std::cout << "PASS native structure: TOC, label sizes, records, depth/count limits, truncation, EOF\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
