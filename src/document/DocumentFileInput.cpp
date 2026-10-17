#include "DocumentFileInput.hpp"
#include "DocumentFormat.hpp"
#include <FSD_BinaryFile.hxx>
#include <algorithm>
#include <array>
#include <istream>
#include <sstream>
#include <string_view>

namespace voelund::document::detail {
namespace {
std::string capture(std::istream& source) {
    if (!source.good())
        invalidDocument("input is not readable");
    std::string bytes;
    std::array<char, 16384> block{};
    for (;;) {
        // Consume at most one byte past the cap, including for an endless stream.
        const auto count = std::min(block.size(), maximumDocumentBytes - bytes.size() + 1);
        try {
            source.read(block.data(), static_cast<std::streamsize>(count));
        } catch (const std::ios_base::failure&) {
            if (source.bad() || !source.eof())
                throw;
        }
        if (source.bad() || (source.fail() && !source.eof()))
            invalidDocument("input read failure");
        const auto obtained = static_cast<std::size_t>(source.gcount());
        if (obtained > maximumDocumentBytes - bytes.size())
            invalidDocument("document byte limit exceeded");
        bytes.append(block.data(), obtained);
        if (source.eof())
            return bytes;
        if (obtained != count)
            invalidDocument("incomplete input read");
    }
}
DocumentFileKind classify(const std::string& bytes) {
    // OCCT's magic selects the binary container family, not an application schema.
    // No native Open, attribute allocation, or legacy fallback occurs here.
    if (std::string_view(bytes).starts_with(FSD_BinaryFile::MagicNumber()))
        return DocumentFileKind::nativeBinary;
    const auto end = bytes.find('\n');
    const auto length = end == std::string::npos ? bytes.size() : end;
    if (length > maximumLineBytes)
        invalidDocument("file header byte limit exceeded");
    std::istringstream header(bytes.substr(0, length));
    (void)readSourceFormat(header); // Exact historical versions, including existing CRLF rules.
    return DocumentFileKind::legacyText;
}
} // namespace
DocumentFileInput DocumentFileInput::read(std::istream& source) {
    auto bytes = capture(source);
    const auto kind = classify(bytes);
    return {kind, std::move(bytes)};
}
} // namespace voelund::document::detail
