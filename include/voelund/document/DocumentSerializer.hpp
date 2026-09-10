#pragma once

#include <iosfwd>

namespace voelund::document {

class ModelDocument;

class DocumentSerializer final {
public:
    static void write(const ModelDocument& document, std::ostream& output);
    [[nodiscard]] static ModelDocument read(std::istream& input);
};

} // namespace voelund::document
