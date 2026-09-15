#include <voelund/document/DocumentId.hpp>

namespace voelund::document {
std::string documentIdToHex(DocumentId id) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(32, '0');
    for (unsigned index = 0; index < 16; ++index) {
        const auto shift = (15 - index) * 4;
        text[index] = digits[(id.high() >> shift) & 15];
        text[index + 16] = digits[(id.low() >> shift) & 15];
    }
    return text;
}

DocumentId documentIdFromHex(std::string_view text) {
    if (text.size() != 32)
        throw std::invalid_argument("Document ID requires 32 lowercase hex digits");
    const auto word = [](std::string_view half) {
        std::uint64_t value = 0;
        for (const char digit : half) {
            if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f')))
                throw std::invalid_argument("Document ID requires lowercase hex digits");
            const auto nibble = digit <= '9' ? digit - '0' : digit - 'a' + 10;
            value = (value << 4) | static_cast<std::uint64_t>(nibble);
        }
        return value;
    };
    return DocumentId{word(text.substr(0, 16)), word(text.substr(16))};
}
} // namespace voelund::document
