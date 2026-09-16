#include "../../src/document/NativeAttributeValues.hpp"
#include "../../src/document/NativeContainerHeader.hpp"
#include "../../src/document/NativeContainerStructure.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Action> void rejected(Action action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    require(failed, "Invalid native attribute accepted");
}
void put(std::string& bytes, std::size_t at, std::uint64_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) bytes.at(at + i) = static_cast<char>((value >> (i * 8)) & 255);
}
std::uint64_t get(const std::string& bytes, std::size_t at, unsigned width = 4) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes.at(at + i))) << (i * 8);
    return value;
}
void valueChecks() {
    using K = NativeAttributeKind;
    require(validateNativeAttributeValue(K::ascii, std::string("Name\0", 5)) == 0, "ASCII rejected");
    rejected([&] { (void)validateNativeAttributeValue(K::ascii, "Name"); });
    rejected([&] { (void)validateNativeAttributeValue(K::ascii, std::string("a\0b\0", 4)); });
    rejected([&] { (void)validateNativeAttributeValue(K::ascii, std::string(4098, '\0')); });
    (void)validateNativeAttributeValue(K::integer, std::string(4, '\0'));
    rejected([&] { (void)validateNativeAttributeValue(K::integer, std::string(20, '\0')); });
    std::string real(8, '\0');
    (void)validateNativeAttributeValue(K::real, real);
    put(real, 0, std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity()), 8);
    rejected([&] { (void)validateNativeAttributeValue(K::real, real); });
    std::string array(17, '\0');
    put(array, 0, 1); put(array, 4, 1);
    (void)validateNativeAttributeValue(K::realArray, array);
    for (const auto upper : {0U, 0xffffffffU, 0x7fffffffU, 2U}) {
        auto changed = array; put(changed, 4, upper);
        rejected([&] { (void)validateNativeAttributeValue(K::realArray, changed); });
    }
    array.back() = 1;
    rejected([&] { (void)validateNativeAttributeValue(K::realArray, array); });
    std::string ref(12, '\0'); put(ref, 0, 2); put(ref, 8, 1);
    require(validateNativeAttributeValue(K::reference, ref) == 2, "Reference budget wrong");
    for (const auto count : {0U, 3U, 257U, 0xffffffffU}) {
        auto changed = ref; put(changed, 0, count);
        rejected([&] { (void)validateNativeAttributeValue(K::reference, changed); });
    }
    put(ref, 8, 0xffffffff);
    rejected([&] { (void)validateNativeAttributeValue(K::reference, ref); });
}
struct Record { std::size_t header; std::size_t size; };
void records(const std::string& bytes, std::size_t& at, std::vector<Record>& out) {
    at += 8;
    while (get(bytes, at) != 0xffffffff) {
        const auto size = static_cast<std::size_t>(get(bytes, at + 8));
        out.push_back({at, size});
        at += 12 + size;
    }
    at += 4;
    while (get(bytes, at) != 0xfffffffe) { at += 4; records(bytes, at, out); }
    at += 4;
}
IdentifiedDocumentSnapshot fixture() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box Ω", {40, 30, 20});
    const auto sketch = model.addSketchFeature("Profile", box,
        {{0, 0, 20, 0, 0, 1, 1, 0, 0}, {{{SketchElementId{1}, SketchRectangle{1, 1, 5, 5}}}, 2}, {{}, 1}});
    (void)model.addExtrusionFeature("Extrude", sketch, {3, {SketchProfileId{1}}});
    return IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
}
void typeTable(const std::string& bytes) {
    const auto ascii = bytes.find("TDataStd_AsciiString");
    require(ascii != std::string::npos, "Missing type-table fixture");
    auto duplicate = bytes;
    const std::string replacement = "TDataStd_Integer";
    const auto previousLength = get(bytes, ascii - 4);
    duplicate.replace(ascii, static_cast<std::size_t>(previousLength), replacement);
    put(duplicate, ascii - 4, replacement.size());
    const auto difference = previousLength - replacement.size();
    for (const auto offset : {15U, 19U, 23U, 55U}) put(duplicate, offset, get(bytes, offset) - difference);
    rejected([&] { (void)readNativeAttributeTypes(duplicate); });
    // Reordering the header changes numeric driver IDs, not the driver's meaning.
    const auto next = ascii + static_cast<std::size_t>(previousLength);
    const auto nextLength = static_cast<std::size_t>(get(bytes, next));
    auto reordered = bytes;
    const auto first = bytes.substr(ascii - 4, 4 + static_cast<std::size_t>(previousLength));
    const auto second = bytes.substr(next, 4 + nextLength);
    reordered.replace(ascii - 4, first.size() + second.size(), second + first);
    const auto types = readNativeAttributeTypes(reordered);
    require(types.values[0] == NativeAttributeKind::integer && types.values[1] == NativeAttributeKind::ascii,
        "Driver mapping ignored header order");
}
void integration() {
    const auto expected = fixture();
    const auto bytes = NativeDocumentStore::encode(expected);
    const auto types = readNativeAttributeTypes(bytes);
    require(types.count == 5, "Fixture does not cover every driver");
    typeTable(bytes);
    require(NativeDocumentStore::decode(bytes) == expected, "Native fields changed");
    auto badType = bytes;
    badType.at(badType.find("TDataStd_AsciiString")) = 'X';
    rejected([&] { (void)NativeDocumentStore::decode(badType); });
    auto external = bytes;
    external.at(external.find("REFERENCE_COUNTER: 0") + 19) = '1';
    rejected([&] { (void)readNativeAttributeTypes(external); });
    std::vector<Record> values;
    auto at = validateNativeContainerHeader(bytes) + 36;
    records(bytes, at, values);
    require(values.size() > 2 && at == bytes.size(), "Fixture traversal failed");
    for (const auto record : values) {
        auto changed = bytes;
        const auto kind = types.values[static_cast<std::size_t>(get(bytes, record.header) - 1)];
        if (kind == NativeAttributeKind::realArray || kind == NativeAttributeKind::reference) {
            put(changed, record.header + 12, 0xffffffff);
            rejected([&] { (void)NativeDocumentStore::decode(changed); });
        }
    }
    auto duplicate = bytes;
    put(duplicate, values[1].header + 4, get(bytes, values[0].header + 4));
    rejected([&] { (void)NativeDocumentStore::decode(duplicate); });
    auto missing = bytes; put(missing, values[0].header, types.count + 1);
    rejected([&] { (void)NativeDocumentStore::decode(missing); });
}
} // namespace
int main(int argc, char** argv) {
    try {
        valueChecks(); integration();
        for (int i = 1; i < argc; ++i) {
            std::ifstream input(argv[i], std::ios::binary);
            require(input.good(), "Missing exchange fixture");
            const std::string bytes{std::istreambuf_iterator<char>(input), {}};
            (void)NativeDocumentStore::decode(bytes);
        }
        std::cout << "PASS native attributes: drivers, values, array bounds, reference lengths, IDs, roundtrip\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
