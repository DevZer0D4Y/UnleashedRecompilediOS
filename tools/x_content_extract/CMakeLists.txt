project("x_content_extract")
set(CMAKE_CXX_STANDARD 20)

add_executable(x_content_extract
    "x_content_extract.cpp"
    "${CMAKE_SOURCE_DIR}/UnleashedRecomp/install/iso_file_system.cpp"
    "${CMAKE_SOURCE_DIR}/UnleashedRecomp/install/xcontent_file_system.cpp"
)

target_include_directories(x_content_extract PRIVATE
    "${CMAKE_SOURCE_DIR}/UnleashedRecomp"
    "${UNLEASHED_RECOMP_TOOLS_ROOT}/XenonRecomp/XenonUtils"
)
