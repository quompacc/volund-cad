#include "../../src/document/NativeContainerHeader.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::uint32_t get(const std::string& bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes.at(at + i)))
                 << (i * 8);
    return value;
}
void put(std::string& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.at(at + i) = static_cast<char>((value >> (i * 8)) & 255);
}
void rejected(const std::string& bytes) {
    bool failed = false;
    try {
        (void)validateNativeContainerHeader(bytes);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Invalid native metadata accepted");
}
void mutate(const std::string& bytes, std::size_t at, std::uint32_t value) {
    auto changed = bytes;
    put(changed, at, value);
    rejected(changed);
}
void regressions() {
    auto model = ModelDocument::empty();
    (void)model.addBoxFeature("Header Ω", {1, 2, 3});
    const auto snapshot =
        IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
    const auto bytes = NativeDocumentStore::encode(snapshot);
    const auto end = validateNativeContainerHeader(bytes);
    require(end > 59 && end < bytes.size(), "Header did not return payload boundary");
    require(NativeDocumentStore::decode(bytes) == snapshot, "Valid native roundtrip rejected");
    for (std::size_t n = 0; n <= end; ++n)
        rejected(bytes.substr(0, n));
    auto foreign = bytes;
    foreign[0] = 'X';
    rejected(foreign);
    mutate(bytes, 7, 0x01020304);  // unsupported byte order
    mutate(bytes, 11, 0);          // beginning of info
    mutate(bytes, 15, UINT32_MAX); // end of info
    mutate(bytes, 19, 0);          // non-contiguous comments
    mutate(bytes, 23, UINT32_MAX); // end of comments
    for (std::size_t field = 5; field <= 11; ++field)
        mutate(bytes, 7 + 4 * field, 0);
    mutate(bytes, 55, UINT32_MAX); // metadata end is not the native file end
    mutate(bytes, 59, UINT32_MAX); // object count, before any OCCT allocation
    std::size_t offset = 63;
    for (const auto width : {1U, 1U, 1U, 1U, 2U, 1U, 2U}) {
        mutate(bytes, offset, UINT32_MAX);
        mutate(bytes, offset, 0x7fffffff);
        offset += 4 + get(bytes, offset) * width;
    }
    const auto users = get(bytes, offset);
    mutate(bytes, offset, UINT32_MAX);
    mutate(bytes, offset, users + 1);
    offset += 4;
    for (std::uint32_t i = 0; i < users; ++i) {
        mutate(bytes, offset, UINT32_MAX);
        offset += 4 + get(bytes, offset);
    }
    require(offset == get(bytes, 15), "Test metadata layout changed");
    mutate(bytes, offset, UINT32_MAX); // comment count
    auto future = bytes;
    future[68] = '3'; // storage version "13", not application schema
    rejected(future);
    bool failed = false;
    try {
        (void)NativeDocumentStore::decode(future);
    } catch (const std::invalid_argument& error) {
        failed = std::string(error.what()).find("container header") != std::string::npos;
    }
    require(failed, "Store did not reject version before native Open");
    // Explicit boundary: this validator does not claim to parse the native payload.
    require(validateNativeContainerHeader(bytes + "trailing") == end,
            "Header checker unexpectedly changed payload contract");
}
} // namespace
int main(int argc, char** argv) {
    try {
        regressions();
        for (int i = 1; i < argc; ++i) {
            std::ifstream input(argv[i], std::ios::binary);
            require(input.good(), "Missing exchange fixture");
            const std::string bytes{std::istreambuf_iterator<char>(input), {}};
            (void)validateNativeContainerHeader(bytes);
            (void)NativeDocumentStore::decode(bytes);
        }
        std::cout
            << "PASS native header: ranges, lengths, counts, version, truncation, roundtrip\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
