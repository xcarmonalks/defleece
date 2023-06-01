function(setup_build)
    target_sources(
        defleece PRIVATE
    )

    if(NOT LITECORE_PREBUILT_LIB STREQUAL "")
        # Very likely that if using prebuilt LiteCore that the versions of libstdc++ differ
        # so use a static libstdc++ to fill in the gaps
        target_link_libraries(
            defleece PRIVATE
            -static-libstdc++
        )
    endif()
endfunction()