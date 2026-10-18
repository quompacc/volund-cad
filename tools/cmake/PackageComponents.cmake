# L04: reads packaging/Components.cmake for the manifest writer and the notice check.
#
#   include(PackageComponents.cmake)   # with VOELUND_COMPONENTS (Components.cmake) and
#                                      # VOELUND_COMPONENT_VERSIONS (configured versions)
#   voelund_component_of(<relative path> <out>)   # component name, or "" if none matches

if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW) # a version not known on this platform stays an empty string
endif()

function(voelund_component name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "EMBEDDED" "VERSION;LICENSE" "PATHS;NOTICES")
    set_property(GLOBAL APPEND PROPERTY voelund_components "${name}")
    set_property(GLOBAL PROPERTY "voelund_component_${name}_version" "${arg_VERSION}")
    set_property(GLOBAL PROPERTY "voelund_component_${name}_license" "${arg_LICENSE}")
    set_property(GLOBAL PROPERTY "voelund_component_${name}_embedded" "${arg_EMBEDDED}")
    set_property(GLOBAL PROPERTY "voelund_component_${name}_paths" "${arg_PATHS}")
    set_property(GLOBAL PROPERTY "voelund_component_${name}_notices" "${arg_NOTICES}")
endfunction()

function(voelund_component_of path out)
    get_property(components GLOBAL PROPERTY voelund_components)
    foreach(name IN LISTS components)
        get_property(patterns GLOBAL PROPERTY "voelund_component_${name}_paths")
        foreach(pattern IN LISTS patterns)
            if(path MATCHES "${pattern}")
                set(${out} "${name}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()
    set(${out} "" PARENT_SCOPE)
endfunction()

# All regular files below the package root, relative and sorted; the manifest itself excluded.
function(voelund_package_files prefix out)
    file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${prefix}" "${prefix}/*")
    list(REMOVE_ITEM files "MANIFEST.txt")
    list(SORT files)
    set(${out} "${files}" PARENT_SCOPE)
endfunction()

foreach(required VOELUND_COMPONENTS VOELUND_COMPONENT_VERSIONS)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "${required} must name an existing file")
    endif()
endforeach()
include("${VOELUND_COMPONENT_VERSIONS}")
include("${VOELUND_COMPONENTS}")
