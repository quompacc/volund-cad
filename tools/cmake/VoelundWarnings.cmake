function(voelund_enable_warnings target_name)
    if(MSVC)
        target_compile_options(
            ${target_name}
            INTERFACE
                /W4
                /permissive-
                /Zc:__cplusplus
                /utf-8
        )
        if(VOELUND_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} INTERFACE /WX)
        endif()
    else()
        target_compile_options(
            ${target_name}
            INTERFACE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Wnon-virtual-dtor
        )
        if(VOELUND_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} INTERFACE -Werror)
        endif()
    endif()
endfunction()
