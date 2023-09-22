

add_executable(
    test_gui
    test_gui.cpp
)

target_include_directories(test_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(test_gui commons )

if(COMMONS_FOR_JS)
    set_target_properties(test_gui PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file fonts -s ALLOW_MEMORY_GROWTH=1") #--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../@fonts/Quicksand-.ttf")
    target_compile_options(test_gui PRIVATE -pthread)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(test_gui PRIVATE -fbracket-depth=400)
endif()

if(APPLE)
set_target_properties(test_gui PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_GUI_IDENTIFIER "app.trex.test_gui"
  MACOSX_BUNDLE_BUNDLE_NAME TRex
  MACOSX_BUNDLE_INFO_STRING "Testing some GUI functions"
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)
endif()

if(NOT COMMONS_NO_PYTHON)
    add_executable(
        test_python
        test_python.cpp
    )

    target_include_directories(test_python PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
    target_link_libraries(test_python commons)
    target_link_libraries(test_python pybind11::pybind11 pybind11::embed pybind11::module)

    if(COMMONS_FOR_JS)
        set_target_properties(test_python PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}") #--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../@fonts/Quicksand-.ttf")
    endif()

    #target_precompile_headers(test_python REUSE_FROM commons)

    install(TARGETS test_python
        EXPORT test_python
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION bin
        INCLUDES DESTINATION include
        CONFIGURATIONS Release
    )
endif()

function(copy_resources EXEC_NAME FILES)
    foreach(comp ${FILES})
        if(IS_DIRECTORY ${comp})
            get_filename_component(comp_s ${comp} NAME)
            if(NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
                add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${comp}"
                        "$<TARGET_FILE_DIR:${EXEC_NAME}>/${comp_s}")
                message(STATUS "Copied directory ${comp} to $<TARGET_FILE_DIR:${EXEC_NAME}>/${comp_s}")
            else()
                add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${comp}"
                        "$<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
                message(STATUS "Copied directory ${comp} to $<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
            endif()
        else()
            get_filename_component(comp_s ${comp} NAME)
            if(NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
                add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                        "${comp}"
                        "$<TARGET_FILE_DIR:${EXEC_NAME}>/${comp_s}")
                message(STATUS "Copied ${comp} to $<TARGET_FILE_DIR:${EXEC_NAME}>/${comp_s}")
            else()
                add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                        "${comp}"
                        "$<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
                message(STATUS "Copied ${comp} to $<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
                add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E create_symlink
                        "${comp}"
                        "$<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
                message(STATUS "Created symlink for ${comp} in $<TARGET_FILE_DIR:${EXEC_NAME}>/../Resources/${comp_s}")
            endif()
        endif()
    endforeach()
endfunction()

set(RESOURCE_COPY_FILES
    ${CMAKE_SOURCE_DIR}/src/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/test_gui.json
)

copy_resources(test_gui "${RESOURCE_COPY_FILES}")