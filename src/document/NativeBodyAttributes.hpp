#pragma once

#include <voelund/document/BodyAttributes.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <TDF_Label.hxx>

#include <span>

namespace voelund::document {

// M01: a table is valid when every key is a body the features create and no entry holds only
// defaults (an unnamed, visible body has no entry). Names: at most maximumBodyNameBytes, no
// NUL. Throws std::invalid_argument otherwise.
void validateBodyAttributes(const BodyAttributeTable& table,
                            std::span<const FeatureDescriptor> features,
                            std::span<const FeatureParameters> parameters);

namespace detail {
// Schema 12, optional child 31 of the main label, written only when the table is not empty.
// Layout: integer = entry count; child i = entry with children 1 body ID (decimal text),
// 2 name (UTF-8 text), 3 hidden (integer 0/1).
inline constexpr int nativeBodyAttributeSlot = 31;
void writeNativeBodyAttributes(const TDF_Label& label, const BodyAttributeTable& table);
// M14 (schema 34), optional child 32 of the main label, written only when a feature is
// suppressed: integer = count; child i = a suppressed feature's ID (decimal text).
inline constexpr int nativeHistoryStateSlot = 32;
[[nodiscard]] BodyAttributeTable readNativeBodyAttributes(const TDF_Label& label);
} // namespace detail

} // namespace voelund::document
