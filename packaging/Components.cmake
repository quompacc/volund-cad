# L04: what the package consists of. Every installed file belongs to exactly one component,
# matched by the regular expressions after PATHS (relative to the package root). A component
# whose files are in the package, or that is compiled into one of them (EMBEDDED), must carry
# its NOTICES. WritePackageManifest.cmake and CheckPackageNotices.cmake read this list; versions
# come from the configured component-versions.cmake.
#
# This is release material, not a legal clearance: see docs/provenance/README.md.

voelund_component(voelund
    VERSION "${VOELUND_VERSION}"
    # User decision 2026-10-18: Völund's own licence is still open; decided before L10.
    LICENSE "LicenseRef-Voelund-undecided"
    PATHS [[^bin/voelund-cad(\.exe)?$]] [[^(register|unregister)-file-type\.ps1$]]
        [[^(install|uninstall)-user\.sh$]] [[^share/applications/]] [[^share/mime/]]
        [[^THIRD-PARTY-NOTICES\.md$]] [[^MANIFEST\.txt$]])

voelund_component(fonts
    VERSION "Albert Sans fffdadf0, JetBrains Mono 6e4b84c9 (google/fonts)"
    LICENSE "OFL-1.1"
    EMBEDDED
    PATHS [[^licenses/fonts/]]
    NOTICES licenses/fonts/AlbertSans-OFL.txt licenses/fonts/JetBrainsMono-OFL.txt)

voelund_component(occt-samples-qt
    VERSION "72e32d3a (adapted in src/viewport/QtOcctGlContext.cpp)"
    LICENSE "MIT"
    EMBEDDED
    PATHS [[^licenses/occt-samples-qt/]]
    NOTICES licenses/occt-samples-qt/LICENSE.txt)

voelund_component(planegcs
    VERSION "FreeCAD 1.1.1 0108fd4b4850cc46e625b60e53cea7a7bbe69f8d"
    LICENSE "LGPL-2.1-or-later"
    PATHS [[^bin/voelund_planegcs\.dll$]] [[^lib(64)?/libvoelund_planegcs\.so$]]
        [[^source/planegcs/]] [[^licenses/planegcs/]]
    NOTICES licenses/planegcs/COPYING.LIB source/planegcs/README-Voelund.md)

voelund_component(eigen
    VERSION "${VOELUND_EIGEN_VERSION}"
    LICENSE "MPL-2.0 AND Apache-2.0 AND BSD-3-Clause AND LicenseRef-MINPACK"
    EMBEDDED
    PATHS [[^licenses/eigen/]]
    NOTICES licenses/eigen/COPYING.README licenses/eigen/COPYING.MPL2
        licenses/eigen/COPYING.APACHE licenses/eigen/COPYING.BSD licenses/eigen/COPYING.MINPACK)

voelund_component(boost
    VERSION "${VOELUND_BOOST_VERSION} (Boost.Graph headers)"
    LICENSE "BSL-1.0"
    EMBEDDED
    PATHS [[^licenses/boost/]]
    NOTICES licenses/boost/LICENSE_1_0.txt)

voelund_component(opencascade
    VERSION "${VOELUND_OCCT_VERSION}"
    LICENSE "LGPL-2.1-only WITH OCCT-exception-1.0"
    PATHS [[^bin/TK[A-Za-z0-9]+\.dll$]] [[^lib(64)?/libTK[A-Za-z0-9]+\.so]]
        [[^licenses/opencascade/]]
    NOTICES licenses/opencascade/LICENSE_LGPL_21.txt licenses/opencascade/OCCT_LGPL_EXCEPTION.txt)

voelund_component(qt
    VERSION "${VOELUND_QT_VERSION}"
    LICENSE "LGPL-3.0-only"
    PATHS [[^bin/Qt6[A-Za-z]+\.dll$]] [[^bin/platforms/q[a-z]+\.dll$]] [[^licenses/qt/]]
    NOTICES licenses/qt/LGPL-3.0-only.txt licenses/qt/GPL-3.0-only.txt)

voelund_component(freetype
    VERSION "${VOELUND_FREETYPE_VERSION}"
    LICENSE "FTL"
    PATHS [[^bin/freetyped?\.dll$]] [[^licenses/freetype/]]
    NOTICES licenses/freetype/copyright.txt)

voelund_component(zlib
    VERSION "${VOELUND_ZLIB_VERSION}"
    LICENSE "Zlib"
    PATHS [[^bin/(zlib1|zlibd1|z|zd)\.dll$]] [[^licenses/zlib/]]
    NOTICES licenses/zlib/copyright.txt)

voelund_component(libpng
    VERSION "${VOELUND_LIBPNG_VERSION}"
    LICENSE "libpng-2.0"
    PATHS [[^bin/libpng16d?\.dll$]] [[^licenses/libpng/]]
    NOTICES licenses/libpng/copyright.txt)

voelund_component(bzip2
    VERSION "${VOELUND_BZIP2_VERSION}"
    LICENSE "bzip2-1.0.6"
    PATHS [[^bin/bz2d?\.dll$]] [[^licenses/bzip2/]]
    NOTICES licenses/bzip2/copyright.txt)

voelund_component(brotli
    VERSION "${VOELUND_BROTLI_VERSION}"
    LICENSE "MIT"
    PATHS [[^bin/brotli(common|dec|enc)d?\.dll$]] [[^licenses/brotli/]]
    NOTICES licenses/brotli/copyright.txt)

# Redistributable under the Visual Studio licence terms; named in THIRD-PARTY-NOTICES.md.
voelund_component(msvc-runtime
    VERSION "${VOELUND_MSVC_VERSION}"
    LICENSE "LicenseRef-MSVC-Redistributable"
    PATHS [[^bin/(msvcp140|vcruntime140|concrt140|vccorlib140)[_a-z0-9]*\.dll$]])
