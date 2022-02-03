

add_executable(
    test_gui
    test_gui.cpp
)

target_include_directories(test_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(test_gui commons Commons::OpenCV )
