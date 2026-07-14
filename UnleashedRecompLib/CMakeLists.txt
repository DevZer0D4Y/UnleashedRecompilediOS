project("UnleashedRecompLib")

set(UNLEASHED_RECOMP_IOS OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS" OR IOS)
    set(UNLEASHED_RECOMP_IOS ON)
endif()

set(UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP OFF)
if (UNLEASHED_RECOMP_IOS)
    set(UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP ON)
endif()

set(UNLEASHED_RECOMP_PPC_BOUNDS_CHECK "AUTO" CACHE STRING
    "Guest-memory bounds checking: AUTO enables it in Debug, ON enables it in every configuration, and OFF disables it")
set_property(CACHE UNLEASHED_RECOMP_PPC_BOUNDS_CHECK PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${UNLEASHED_RECOMP_PPC_BOUNDS_CHECK}" UNLEASHED_RECOMP_PPC_BOUNDS_CHECK_MODE)
if (NOT UNLEASHED_RECOMP_PPC_BOUNDS_CHECK_MODE MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "UNLEASHED_RECOMP_PPC_BOUNDS_CHECK must be AUTO, ON, or OFF")
endif()

add_compile_options(
    -fno-strict-aliasing
)

if (WIN32)
    add_compile_options(/fp:strict)
else()
    add_compile_options(-ffp-model=strict)
endif()

if (NOT UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP)
    target_compile_definitions(XenonRecomp PRIVATE 
        XENON_RECOMP_CONFIG_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml"
        XENON_RECOMP_HEADER_FILE_PATH="${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp/XenonUtils/ppc_context.h")
endif()

set(UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp_shared.h"
)

foreach(i RANGE 0 260)
    list(APPEND UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.${i}.cpp")
endforeach()

if (UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP)
    set(UNLEASHED_RECOMP_MISSING_GENERATED_SOURCES)
    foreach(GENERATED_SOURCE IN LISTS UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES)
        if (NOT EXISTS "${GENERATED_SOURCE}")
            list(APPEND UNLEASHED_RECOMP_MISSING_GENERATED_SOURCES "${GENERATED_SOURCE}")
        endif()
    endforeach()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp")
        list(APPEND UNLEASHED_RECOMP_MISSING_GENERATED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp")
    endif()

    list(LENGTH UNLEASHED_RECOMP_MISSING_GENERATED_SOURCES UNLEASHED_RECOMP_MISSING_GENERATED_SOURCE_COUNT)
    if (UNLEASHED_RECOMP_MISSING_GENERATED_SOURCE_COUNT GREATER 0)
        list(GET UNLEASHED_RECOMP_MISSING_GENERATED_SOURCES 0 UNLEASHED_RECOMP_FIRST_MISSING_GENERATED_SOURCE)
        message(FATAL_ERROR
            "The iOS build requires pre-generated guest code (${UNLEASHED_RECOMP_MISSING_GENERATED_SOURCE_COUNT} files are missing; first: ${UNLEASHED_RECOMP_FIRST_MISSING_GENERATED_SOURCE}).\n"
            "Place default.xex, default.xexp, and shader.ar in UnleashedRecompLib/private, then run:\n"
            "  cmake --fresh --preset macos-debug\n"
            "  cmake --build out/build/macos-debug --target file_to_c UnleashedRecompGenerateArtifacts")
    endif()
endif()

if (NOT UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP)
    add_custom_command(
        OUTPUT 
            "${CMAKE_CURRENT_SOURCE_DIR}/private/default_patched.xex"
            ${UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES}
        COMMAND 
            $<TARGET_FILE:XenonRecomp>
        DEPENDS 
            "${CMAKE_CURRENT_SOURCE_DIR}/private/default.xex"
            "${CMAKE_CURRENT_SOURCE_DIR}/private/default.xexp"
            "${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml"
        USES_TERMINAL
    )

    add_custom_command(
        OUTPUT
            "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar"
        COMMAND
            $<TARGET_FILE:x_decompress> "${CMAKE_CURRENT_SOURCE_DIR}/private/shader.ar" "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/private/shader.ar"
    )
endif()

set(XENOS_RECOMP_ROOT "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenosRecomp/XenosRecomp")
set(XENOS_RECOMP_INCLUDE "${XENOS_RECOMP_ROOT}/shader_common.h")

if (NOT UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP)
    target_compile_definitions(XenosRecomp PRIVATE 
        XENOS_RECOMP_INPUT="${CMAKE_CURRENT_SOURCE_DIR}/private" 
        XENOS_RECOMP_OUTPUT="${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp"
        XENOS_RECOMP_INCLUDE_INPUT="${XENOS_RECOMP_INCLUDE}"
        UNLEASHED_RECOMP
    )
endif()

file(GLOB XENOS_RECOMP_SOURCES 
    "${XENOS_RECOMP_ROOT}/*.h"
    "${XENOS_RECOMP_ROOT}/*.cpp"
)

if (NOT UNLEASHED_RECOMP_USE_PREGENERATED_RECOMP)
    add_custom_command(
        OUTPUT 
            "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp"
        COMMAND
            $<TARGET_FILE:XenosRecomp>
        DEPENDS 
            "${CMAKE_CURRENT_SOURCE_DIR}/private/default_patched.xex"
            "${CMAKE_CURRENT_SOURCE_DIR}/private/shader_decompressed.ar" 
            ${XENOS_RECOMP_SOURCES} 
            ${XENOS_RECOMP_INCLUDE}
        USES_TERMINAL
    )

    add_custom_target(UnleashedRecompGenerateArtifacts
        DEPENDS
            ${UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp"
    )
endif()

add_library(UnleashedRecompLib 
    ${UNLEASHED_RECOMP_PPC_RECOMPILED_SOURCES}
    "shader/shader_cache.h"
    "shader/shader_cache.cpp"
)

if (UNLEASHED_RECOMP_IOS)
    if (UNLEASHED_RECOMP_PPC_BOUNDS_CHECK_MODE STREQUAL "ON")
        target_compile_definitions(UnleashedRecompLib PUBLIC UNLEASHED_RECOMP_PPC_BOUNDS_CHECK)
    elseif (UNLEASHED_RECOMP_PPC_BOUNDS_CHECK_MODE STREQUAL "AUTO")
        target_compile_definitions(UnleashedRecompLib PUBLIC
            "$<$<CONFIG:Debug>:UNLEASHED_RECOMP_PPC_BOUNDS_CHECK>")
    endif()
endif()

target_include_directories(UnleashedRecompLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(UnleashedRecompLib PRIVATE "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp/thirdparty/simde")
target_precompile_headers(UnleashedRecompLib PUBLIC "ppc/ppc_recomp_shared.h")
