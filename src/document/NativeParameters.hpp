#pragma once
#include <TDF_Label.hxx>
#include <voelund/document/DocumentParameters.hpp>

namespace voelund::document::detail {
// PAR01, schema 7: Main() slot of the parameter table. Written only for a non-default
// table, so documents without parameters keep their older encoding.
inline constexpr int nativeParameterSlot = 30;
// Records the table consumes from the document budget.
[[nodiscard]] std::size_t nativeParameterRecords(const ParameterTable& table) noexcept;
void writeNativeParameterTable(const TDF_Label& label, const ParameterTable& table);
[[nodiscard]] ParameterTable readNativeParameterTable(const TDF_Label& label,
                                                      std::size_t remainingRecords);
// Optional binding child of a constraint or extrusion payload: the decimal parameter ID.
void writeNativeParameterBinding(const TDF_Label& label, ParameterId id);
[[nodiscard]] ParameterId readNativeParameterBinding(const TDF_Label& label);
} // namespace voelund::document::detail
