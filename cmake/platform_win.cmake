function(setup_build)
    set(BIN_TOP "${PROJECT_BINARY_DIR}/../..")
    if(NOT LITECORE_PREBUILT_LIB STREQUAL "")
        cmake_path(REMOVE_EXTENSION LITECORE_PREBUILT_LIB OUTPUT_VARIABLE FilesToCopy)
    else()
        set(FilesToCopy ${BIN_TOP}/\$\(Configuration\)/LiteCore)
    endif()

    target_compile_definitions(
        defleece PRIVATE
        -DNOMINMAX   # Disable min/max macros (they interfere with std::min and max)
    )

    target_include_directories(
        defleece PRIVATE
        vendor/fleece/MSVC
    )

    add_custom_command(
        TARGET defleece POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -DFilesToCopy="${FilesToCopy}"
        -DDestinationDirectory=${PROJECT_BINARY_DIR}/\$\(Configuration\)
        -P ${TOP}MSVC/copy_artifacts.cmake
    )
endfunction()