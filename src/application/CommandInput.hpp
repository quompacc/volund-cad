#pragma once
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace voelund::application::detail {
template <class T> struct PreparedInput {
    std::optional<T> value;
    std::string diagnostic;
};

// Only wrap document input preparation, never evaluation or publication.
// Allocation, overflow and unexpected exceptions retain their original type.
template <class Prepare>
PreparedInput<std::invoke_result_t<Prepare>> prepareInput(Prepare&& prepare) {
    try {
        return {std::invoke(std::forward<Prepare>(prepare)), {}};
    } catch (const std::invalid_argument& error) {
        return {std::nullopt, error.what()};
    } catch (const std::out_of_range& error) {
        return {std::nullopt, error.what()};
    }
}
} // namespace voelund::application::detail
