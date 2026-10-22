# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# K01 layer rules (docs/architecture-and-code-contract.md section 4), shared by the include
# check (CheckLayerIncludes.cmake) and the link check (voelund_check_layer_links).
#
# A layer may include and link only the layers listed for it. desktop is the top of the
# product and may use every layer; apps and tests are not layers.
set(VOELUND_LAYERS kernel platform document features application workspaces modules viewport
    mcp assistant desktop)

set(VOELUND_LAYER_ALLOWED_kernel kernel)
set(VOELUND_LAYER_ALLOWED_platform platform)
set(VOELUND_LAYER_ALLOWED_document document kernel platform)
set(VOELUND_LAYER_ALLOWED_features features document kernel platform)
set(VOELUND_LAYER_ALLOWED_application application features document kernel platform)
set(VOELUND_LAYER_ALLOWED_workspaces workspaces)
set(VOELUND_LAYER_ALLOWED_modules modules workspaces application features document kernel platform)
set(VOELUND_LAYER_ALLOWED_viewport viewport application features document kernel platform)
set(VOELUND_LAYER_ALLOWED_mcp mcp application features document kernel platform)
set(VOELUND_LAYER_ALLOWED_assistant assistant application features document kernel platform)
set(VOELUND_LAYER_ALLOWED_desktop ${VOELUND_LAYERS})

# Layers below the user interface know no Qt at all (no widgets, dialogs or event loop).
set(VOELUND_LAYERS_WITHOUT_QT kernel platform document features application workspaces modules)

# Link targets of each layer and the CMake aliases other targets use for it.
set(VOELUND_LAYER_TARGET_kernel voelund_kernel)
set(VOELUND_LAYER_TARGET_platform voelund_platform)
set(VOELUND_LAYER_TARGET_document voelund_document)
set(VOELUND_LAYER_TARGET_features voelund_features)
set(VOELUND_LAYER_TARGET_application voelund_application)
set(VOELUND_LAYER_TARGET_workspaces voelund_workspaces)
set(VOELUND_LAYER_TARGET_modules voelund_module_solid)
set(VOELUND_LAYER_TARGET_viewport voelund_viewport)
set(VOELUND_LAYER_TARGET_mcp voelund_mcp)
set(VOELUND_LAYER_TARGET_assistant voelund_assistant)
set(VOELUND_LAYER_TARGET_desktop voelund_desktop)

set(VOELUND_LAYER_ALIAS_Kernel kernel)
set(VOELUND_LAYER_ALIAS_Platform platform)
set(VOELUND_LAYER_ALIAS_Document document)
set(VOELUND_LAYER_ALIAS_Features features)
set(VOELUND_LAYER_ALIAS_Application application)
set(VOELUND_LAYER_ALIAS_Workspaces workspaces)
set(VOELUND_LAYER_ALIAS_ModuleSolid modules)
set(VOELUND_LAYER_ALIAS_Viewport viewport)
set(VOELUND_LAYER_ALIAS_Mcp mcp)
set(VOELUND_LAYER_ALIAS_Assistant assistant)
set(VOELUND_LAYER_ALIAS_Desktop desktop)

# Layer of a link item (Voelund::X alias or voelund_x target), empty when it is none.
function(voelund_layer_of_link item out)
    set(layer "")
    # Two steps: ${CMAKE_MATCH_1} in the same if() would expand before MATCHES sets it.
    if(item MATCHES "^Voelund::(.+)$")
        set(alias "${CMAKE_MATCH_1}")
        if(DEFINED VOELUND_LAYER_ALIAS_${alias})
            set(layer "${VOELUND_LAYER_ALIAS_${alias}}")
        endif()
    else()
        foreach(candidate IN LISTS VOELUND_LAYERS)
            if(item STREQUAL "${VOELUND_LAYER_TARGET_${candidate}}")
                set(layer "${candidate}")
            endif()
        endforeach()
    endif()
    set(${out} "${layer}" PARENT_SCOPE)
endfunction()

# Configure-time check of the link edges between existing layer targets.
function(voelund_check_layer_links)
    set(violations "")
    foreach(layer IN LISTS VOELUND_LAYERS)
        set(target "${VOELUND_LAYER_TARGET_${layer}}")
        if(NOT TARGET "${target}")
            continue()
        endif()
        get_target_property(links "${target}" LINK_LIBRARIES)
        get_target_property(interface_links "${target}" INTERFACE_LINK_LIBRARIES)
        foreach(item IN LISTS links interface_links)
            voelund_layer_of_link("${item}" used)
            if(NOT used STREQUAL "" AND NOT used IN_LIST VOELUND_LAYER_ALLOWED_${layer})
                list(APPEND violations "${target} (${layer}) links ${item} (${used})")
            endif()
        endforeach()
    endforeach()
    if(violations)
        list(REMOVE_DUPLICATES violations)
        list(JOIN violations "\n  " text)
        message(FATAL_ERROR "Forbidden layer link (K01):\n  ${text}")
    endif()
endfunction()
