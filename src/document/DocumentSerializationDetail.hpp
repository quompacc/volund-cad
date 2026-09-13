#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace voelund::document::detail {

inline constexpr std::size_t maximumFeatureCount = 10'000;
inline constexpr std::size_t maximumSketchElementCount = 1'000'000;
inline constexpr std::size_t maximumSketchConstraintCount = 2'000'000;
inline constexpr std::size_t maximumExtrusionProfileCount = 1'000'000;
inline constexpr std::size_t maximumDocumentBytes = 64 * 1024 * 1024;
inline constexpr std::size_t maximumLineBytes = 64 * 1024;
inline constexpr std::size_t maximumNameBytes = 4096;
inline constexpr std::size_t maximumDocumentRecords = 200'000;
inline constexpr std::size_t maximumFeatureDepth = 256;

struct UnsignedValue {
    std::uint64_t& value;
};
UnsignedValue unsignedValue(std::uint64_t& value);
std::istream& operator>>(std::istream& input, UnsignedValue value);

[[noreturn]] void invalidDocument(const std::string& reason);
std::string requiredLine(std::istream& input, const char* description);
std::uint64_t headerValue(std::istream& input, const char* expectedName);
kernel::RigidTransform readTransform(std::istringstream& line);
void validateTransform(const kernel::RigidTransform& transform);
void validatePlaneDirections(const SketchPlane& plane);
void validateConstraintVersion(SketchConstraintKind kind, std::uint64_t version);
[[nodiscard]] bool positiveFinite(double value);
CylinderResultMode readResultMode(std::istringstream& line);
const char* resultModeName(CylinderResultMode mode);
SketchLineConstraint readLineConstraint(std::istringstream& line);
const char* lineConstraintName(SketchLineConstraint constraint);
SketchPointPosition readPointPosition(std::istringstream& line);
const char* pointPositionName(SketchPointPosition position);
void writeTransform(std::ostream& output, const kernel::RigidTransform& transform);
[[nodiscard]] bool containsFeature(const std::vector<FeatureDescriptor>& features,
                                   FeatureId featureId);
[[nodiscard]] bool containsFeatureKind(const std::vector<FeatureDescriptor>& features,
                                       FeatureId featureId, FeatureKind kind);
void ensureLineComplete(std::istringstream& line);

} // namespace voelund::document::detail
