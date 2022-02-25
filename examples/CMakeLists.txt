

add_executable(
    test_gui
    test_gui.cpp
)

target_include_directories(test_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(test_gui commons Commons::OpenCV )

if(COMMONS_FOR_JS)
set_target_properties(test_gui PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}") #--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../@fonts/Quicksand-.ttf")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_gui PRIVATE -fbracket-depth=400)
endif()

#target_precompile_headers(test_gui REUSE_FROM commons)
