// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "DocumentSerializationDetail.hpp"
#include <istream>
#include <streambuf>

namespace voelund::document::detail {
// Read-only streaming boundary: no seek, file-size trust or unbounded getline allocation.
class DocumentInputBuffer final : public std::streambuf {
  public:
    explicit DocumentInputBuffer(std::istream& source) : source_(source) {}

  protected:
    int_type underflow() override {
        if (gptr() && gptr() < egptr())
            return traits_type::to_int_type(*gptr());
        int_type value = traits_type::eof();
        try {
            value = source_.get();
        } catch (const std::ios_base::failure&) {
            if (source_.bad() || !source_.eof())
                throw;
        }
        if (value == traits_type::eof()) {
            if (source_.bad() || !source_.eof())
                invalidDocument("input read failure");
            return traits_type::eof();
        }
        if (++bytes_ > maximumDocumentBytes)
            invalidDocument("document byte limit exceeded");
        character_ = traits_type::to_char_type(value);
        if (character_ == '\0')
            invalidDocument("NUL byte in text document");
        if (lineBytes_ == 0 && ++records_ > maximumDocumentRecords)
            invalidDocument("document record limit exceeded");
        if (character_ == '\n') {
            lineBytes_ = 0;
        } else if (++lineBytes_ > maximumLineBytes) {
            invalidDocument("line byte limit exceeded");
        }
        setg(&character_, &character_, &character_ + 1);
        return value;
    }

  private:
    std::istream& source_;
    char character_{};
    std::size_t bytes_{};
    std::size_t lineBytes_{};
    std::size_t records_{};
};

class BoundedDocumentInput final : public std::istream {
  public:
    explicit BoundedDocumentInput(std::istream& source) : std::istream(nullptr), buffer_(source) {
        rdbuf(&buffer_);
        exceptions(std::ios::badbit);
    }

  private:
    DocumentInputBuffer buffer_;
};
} // namespace voelund::document::detail
