#pragma once
#include "DocumentSerializationDetail.hpp"
#include <algorithm>
#include <cstring>
#include <streambuf>
#include <string>
#include <stdexcept>
#include <utility>

namespace voelund::document::detail {
// Seekable output for BinOcaf's size/offset backpatches. No direct put area:
// every write checks the logical byte cap before growing the string.
class BoundedNativeOutput final : public std::streambuf {
  public:
    explicit BoundedNativeOutput(std::size_t limit = maximumDocumentBytes) : limit_(limit) {
        if (limit > maximumDocumentBytes) throw std::invalid_argument("Native output limit exceeds file policy");
    }
    [[nodiscard]] const std::string& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::string release() { position_ = 0; return std::exchange(bytes_, {}); }
  protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        if (count < 0 || static_cast<std::uint64_t>(count) > limit_ - position_)
            throw std::length_error("Native output byte limit exceeded");
        const auto size = static_cast<std::size_t>(count);
        const auto end = position_ + size;
        if (end > bytes_.size()) bytes_.resize(end);
        if (size) std::memcpy(bytes_.data() + position_, data, size);
        position_ = end;
        return count;
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        const auto character = traits_type::to_char_type(value);
        (void)xsputn(&character, 1);
        return value;
    }
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                     std::ios_base::openmode mode) override {
        if (mode != std::ios_base::out) return pos_type(off_type(-1));
        std::size_t base = 0;
        if (direction == std::ios_base::cur) base = position_;
        else if (direction == std::ios_base::end) base = bytes_.size();
        else if (direction != std::ios_base::beg) return pos_type(off_type(-1));
        if (offset < -static_cast<off_type>(base) || offset > static_cast<off_type>(bytes_.size() - base))
            return pos_type(off_type(-1));
        position_ = static_cast<std::size_t>(static_cast<off_type>(base) + offset);
        return pos_type(static_cast<off_type>(position_));
    }
    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override {
        return seekoff(static_cast<off_type>(position), std::ios_base::beg, mode);
    }
  private:
    std::size_t limit_;
    std::size_t position_{};
    std::string bytes_;
};
} // namespace voelund::document::detail
