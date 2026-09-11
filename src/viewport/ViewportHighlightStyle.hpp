#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <AIS_InteractiveContext.hxx>

namespace voelund::viewport {

void configureHighlightStyles(const occ::handle<AIS_InteractiveContext>& context);

} // namespace voelund::viewport
