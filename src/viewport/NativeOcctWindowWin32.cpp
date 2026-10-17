#include "NativeOcctWindow.hpp"

#include <Aspect_Handle.hxx>
#include <Quantity_NameOfColor.hxx>
#include <WNT_Window.hxx>

namespace voelund::viewport::platform {

occ::handle<Aspect_Window>
makeOcctWindow(const occ::handle<Aspect_DisplayConnection>& displayConnection,
               std::uintptr_t nativeWindowId) {
    static_cast<void>(displayConnection);
    return new WNT_Window(reinterpret_cast<Aspect_Handle>(nativeWindowId), Quantity_NOC_MATRAGRAY);
}

} // namespace voelund::viewport::platform
