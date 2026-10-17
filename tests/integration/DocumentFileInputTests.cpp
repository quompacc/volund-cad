#include "../../src/document/DocumentFileInput.hpp"
#include "../../src/document/DocumentSerializationDetail.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string_view>
#include <voelund/document/DocumentSerializer.hpp>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
using namespace std::string_literals;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, "Invalid input accepted");
}
class ForwardBuffer final : public std::streambuf {
  public:
    explicit ForwardBuffer(std::string bytes, bool failAtEnd = false)
        : bytes_(std::move(bytes)), failAtEnd_(failAtEnd) {
        setg(bytes_.data(), bytes_.data(), bytes_.data() + bytes_.size());
    }

  protected:
    int_type underflow() override {
        if (failAtEnd_)
            throw std::ios_base::failure("Injected input failure");
        return traits_type::eof();
    }

  private:
    std::string bytes_;
    bool failAtEnd_;
};
class GeneratedBuffer final : public std::streambuf {
  public:
    explicit GeneratedBuffer(std::size_t length) : remaining_(length) {}
    std::size_t consumed{};

  protected:
    std::streamsize xsgetn(char* output, std::streamsize requested) override {
        const auto count = std::min(remaining_, static_cast<std::size_t>(requested));
        std::memset(output, ' ', count);
        constexpr std::string_view header = "VOLUND 16\n";
        if (consumed == 0 && count >= header.size())
            std::memcpy(output, header.data(), header.size());
        remaining_ -= count;
        consumed += count;
        return static_cast<std::streamsize>(count);
    }

  private:
    std::size_t remaining_;
};
DocumentFileInput capture(const std::string& bytes, bool throwingEof = false) {
    ForwardBuffer buffer(bytes);
    std::istream input(&buffer);
    if (throwingEof)
        input.exceptions(std::ios::failbit | std::ios::badbit);
    return DocumentFileInput::read(input);
}
void fixtures(const char* root) {
    for (int version = 1; version <= 16; ++version) {
        std::ostringstream path;
        path << root << "/v" << std::setw(2) << std::setfill('0') << version << ".volund";
        std::ifstream file(path.str(), std::ios::binary);
        require(file.good(), "Missing legacy fixture");
        const std::string bytes{std::istreambuf_iterator<char>(file), {}};
        const auto captured = capture(bytes, version % 2 == 0);
        require(captured.kind == DocumentFileKind::legacyText && captured.bytes == bytes,
                "Legacy capture changed bytes or format");
        std::istringstream before(bytes), after(captured.bytes);
        const auto original = DocumentSerializer::readWithReport(before);
        const auto restored = DocumentSerializer::readWithReport(after);
        require(original.snapshot == restored.snapshot && original.report == restored.report,
                "Capture changed legacy interpretation");
    }
}
void boundaries() {
    for (const std::string text : {"", "VOLUND 0\n", "VOLUND 17\n", "VOLUND 01\n", "VOLUND 16x\n",
                                   "VOLUND 16 \n", "unknown\n"})
        rejected([&] { (void)capture(text); });
    rejected([&] { (void)capture(std::string(maximumLineBytes + 1, 'x')); });
    const std::string crlf = "VOLUND 16\r\nPayload\0bytes"s;
    require(capture(crlf).bytes == crlf, "Capture normalized or truncated bytes");
    for (const auto length : {std::size_t{16384}, maximumDocumentBytes}) {
        GeneratedBuffer buffer(length);
        std::istream input(&buffer);
        input.exceptions(std::ios::failbit | std::ios::badbit);
        const auto captured = DocumentFileInput::read(input);
        require(captured.bytes.size() == length, "Exact size boundary rejected");
    }
    GeneratedBuffer endless(maximumDocumentBytes * 2);
    std::istream oversized(&endless);
    rejected([&] { (void)DocumentFileInput::read(oversized); });
    require(endless.consumed == maximumDocumentBytes + 1,
            "Reader consumed beyond overflow sentinel");
    for (bool exceptions : {false, true}) {
        ForwardBuffer broken("VOLUND 16\n", true);
        std::istream input(&broken);
        if (exceptions)
            input.exceptions(std::ios::badbit);
        rejected([&] { (void)DocumentFileInput::read(input); });
    }
    std::istringstream failed("VOLUND 16\n");
    failed.setstate(std::ios::failbit);
    rejected([&] { (void)DocumentFileInput::read(failed); });
}
void native() {
    auto model = ModelDocument::empty();
    (void)model.addBoxFeature("Body", {2, 3, 4});
    const auto expected =
        IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
    const auto bytes = NativeDocumentStore::encode(expected);
    const auto input = capture(bytes, true);
    require(input.kind == DocumentFileKind::nativeBinary && input.bytes == bytes,
            "Native payload was decoded as text or altered");
    require(NativeDocumentStore::decode(input.bytes) == expected,
            "Captured native snapshot changed");
    // Recognition selects a decoder, not a valid model: corruption cannot fall back to text.
    const auto truncated = capture(bytes.substr(0, 32));
    require(truncated.kind == DocumentFileKind::nativeBinary, "Broken native signature fell back");
    rejected([&] { (void)NativeDocumentStore::decode(truncated.bytes); });
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Fixture directory required");
        fixtures(argv[1]);
        boundaries();
        native();
        std::cout << "PASS file input: 16 legacy versions, native, forward streams, EOF, I/O "
                     "errors, size limits\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
