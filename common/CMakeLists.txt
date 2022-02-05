set(RESOURCES_ADDED "")

file(GLOB HDRS video/*.h processing/*.h misc/*.h file/*.h gui/*.h gui/types/*.h *.h)
file(GLOB SRCS video/*.cpp processing/*.cpp misc/*.cpp file/*.cpp gui/*.cpp gui/types/*.cpp *.cpp)

file(GLOB HASH_HDR ${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map/*.hpp)

message(STATUS "hash_map: ${HASH_HDR} ${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map")

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" AND METAL_LIBRARY)
    set(SRCS ${SRCS} gui/MetalImpl.mm)
else()
    message(STATUS "Skipping metal implementation")
endif()

if(APPLE)
   set(SRCS ${SRCS} gui/MacProgressBar.mm)
   message(STATUS "Adding MacProgressBar")
endif()

if(COMMONS_BUILD_HTTPD)
	file(GLOB _HDRS http/*.h)
	file(GLOB _SRCS http/*.cpp)
	
	set(HDRS ${HDRS} ${_HDRS})
	set(SRCS ${SRCS} ${_SRCS})
endif()

# automatic moc file generation by Qt MOC
set(CMAKE_AUTOMOC OFF)
# to include generated moc headers, since they are generated in binary dir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
    set(HDRS ${HDRS} commons.pc.h)
endif()

add_library(commons STATIC ${SRCS} ${HDRS} ${HASH_HDR})
add_dependencies(commons Commons::LibZIP Commons::LibZ Commons::GLFW Commons::OpenGL cnpy Commons::OpenCV Commons::LibPNG)
target_include_directories(commons PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map")

foreach(FILE ${HASH_HDR})
    source_group("hash_map" FILES "${FILE}")
endforeach()
    

foreach(FILE ${SRCS} ${HDRS}) 
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

    # Remove common directory prefix to make the group
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

    # Make sure we are using windows slashes
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    # Group into "Source Files" and "Header Files"
    if ("${FILE}" MATCHES ".*\\.cpp")
       set(GROUP "${GROUP}")
    elseif("${FILE}" MATCHES ".*\\.h")
       set(GROUP "${GROUP}")
    endif()

    source_group("${GROUP}" FILES "${FILE}")
endforeach()

target_link_libraries(commons
    PUBLIC cpputils
    PUBLIC cnpy
    PUBLIC Commons::IMGUI 
    PUBLIC Commons::OpenGL 
    PUBLIC Commons::LibPNG 
    PUBLIC Commons::LibZIP 
    PUBLIC Commons::LibZ
)

option(COMMONS_DONT_USE_PCH OFF)
if(${CMAKE_VERSION} VERSION_LESS "3.16.0" OR COMMONS_DONT_USE_PCH)
    message("Please consider to switch to CMake 3.16.0")
else()
    target_precompile_headers(commons
      PUBLIC
        commons.pc.h
      PRIVATE
    )
endif()

target_compile_definitions(commons PUBLIC COMMONS_HAS_OPENGL=${CMN_FOUND_OPENGL})
if(CMN_USE_OPENGL2)
    target_compile_definitions(commons PUBLIC CMN_USE_OPENGL2=true)
else()
    target_compile_definitions(commons PUBLIC CMN_USE_OPENGL2=false)
endif()

if(APPLE)
  target_compile_options(commons PRIVATE "-DNS_FORMAT_ARGUMENT(A)=" "-D_Nullable_result=_Nullable")
endif()

if(COMMONS_BUILD_HTTPD)
	target_link_libraries(commons PUBLIC Commons::HTTPD)
    add_dependencies(commons Commons::HTTPD)
endif()

add_dependencies(commons Commons::OpenCV)

#
set(RESOURCE_COPY_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonts
)
set(RESOURCE_COPY_SINGLE_FILES
)

set(COMMONS_LIBRARY_NAME commons)

IF("${CMAKE_SYSTEM}" MATCHES "Linux")
    message(STATUS "Copying files ${RESOURCE_COPY_SINGLE_FILES}")
    foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${comp}"
                "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
        message(STATUS ${CMAKE_COMMAND} -E create_symlink
            "${comp}"
            "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
    endforeach()

    foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${comp}"
                "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
    endforeach()
elseif(WIN32)
    message(STATUS "(windows) Copying files ${RESOURCE_COPY_SINGLE_FILES}")
    message(STATUS "(install_prefix) :${CMAKE_INSTALL_PREFIX} :${CMAKE_BINARY_DIR}")
    
    foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${comp}"
                "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
    endforeach()

    message(STATUS "(windows) Copying folders ${RESOURCE_COPY_FILES}")
    foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)

        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${comp}"
                "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
    endforeach()
    
else()
    message(STATUS "Copying files ${RESOURCE_COPY_SINGLE_FILES}")
    foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
        get_filename_component(comp_s ${comp} NAME)
        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                        "${comp}"
                        "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/../Resources/${comp_s}")
        install(FILES ${comp} DESTINATION bin)
    endforeach()

    foreach(comp ${RESOURCE_COPY_FILES})
        get_filename_component(comp_s ${comp} NAME)
        add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${comp}"
                "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/../Resources/${comp_s}")
    endforeach()
endif()
