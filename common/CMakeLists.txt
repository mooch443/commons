set(RESOURCES_ADDED "")

file(GLOB HDRS video/*.h processing/*.h misc/*.h file/*.h gui/*.h gui/types/*.h gui/dyn/*.h *.h)
file(GLOB SRCS video/*.cpp processing/*.cpp misc/*.cpp file/*.cpp gui/*.cpp gui/types/*.cpp gui/dyn/*.cpp *.cpp)

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

# to include generated moc headers, since they are generated in binary dir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(commons STATIC ${SRCS} ${HDRS} ${HASH_HDR})
add_dependencies(commons Commons::LibZIP Commons::LibZ Commons::GLFW Commons::OpenGL cnpy Commons::OpenCV Commons::LibPNG Commons::GLAD)
target_include_directories(commons PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map" 
    "${CMAKE_CURRENT_SOURCE_DIR}/../robin-hood-hashing/src/include")

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
    PUBLIC cnpy
    PUBLIC Commons::OpenCV
    PUBLIC Commons::IMGUI 
    PUBLIC Commons::GLFW
    PUBLIC Commons::FFMPEG
    PUBLIC Commons::OpenGL 
    PUBLIC Commons::LibPNG 
    PUBLIC Commons::LibZIP 
    PUBLIC Commons::LibZ
    PUBLIC nlohmann_json::nlohmann_json
)

option(COMMONS_DONT_USE_PCH OFF)
if(${CMAKE_VERSION} VERSION_LESS "3.16.0" OR COMMONS_DONT_USE_PCH)
    message("Please consider to switch to CMake 3.16.0")
else()
    target_precompile_headers(commons
      PUBLIC
        commons.pc.h
      PUBLIC
    )
endif()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    set(COMMONS_MACOS_VERSION "NOT-FOUND")
    if(CMAKE_OSX_DEPLOYMENT_TARGET AND NOT ${CMAKE_OSX_DEPLOYMENT_TARGET} MATCHES "")
        message(STATUS "Found OSX deployment target: '${CMAKE_OSX_DEPLOYMENT_TARGET}'")
        set(COMMONS_MACOS_VERSION ${CMAKE_OSX_DEPLOYMENT_TARGET})
    else()
        message(STATUS "Need to detect OSX version.")
        if(NOT CMAKE_OSX_SYSROOT)
            message(FATAL_ERROR "No OSX sysroot was set.")
        else()
            get_filename_component(osx_dir ${CMAKE_OSX_SYSROOT} NAME)
            string(REPLACE ".sdk" "" osx_dir ${osx_dir})
            string(REPLACE "MacOSX" "" osx_dir ${osx_dir})
            message(STATUS "OSX Target: ${osx_dir}")
            set(COMMONS_MACOS_VERSION ${osx_dir})
            string(REPLACE "." ";" VERSION_LIST ${osx_dir})
            list(GET VERSION_LIST 0 COMMONS_MACOS_VERSION_MAJOR)
            list(GET VERSION_LIST 1 COMMONS_MACOS_VERSION_MINOR)
            target_compile_definitions(commons PUBLIC COMMONS_MACOS_VERSION_MAJOR=${COMMONS_MACOS_VERSION_MAJOR})
            target_compile_definitions(commons PUBLIC COMMONS_MACOS_VERSION_MINOR=${COMMONS_MACOS_VERSION_MINOR})
            message(STATUS "OSX Major: ${COMMONS_MACOS_VERSION_MAJOR}, Minor: ${COMMONS_MACOS_VERSION_MINOR}")
        endif()
    endif()
endif()

try_compile(HAVE_CONCEPT_IMPLEMENTATION "${CMAKE_BINARY_DIR}/temp" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_concepts.cpp"
             CXX_STANDARD 20
             CXX_STANDARD_REQUIRED TRUE)
if(${HAVE_CONCEPT_IMPLEMENTATION})
    target_compile_definitions(commons PUBLIC HAVE_CONCEPT_IMPLEMENTATION=true)
    message(STATUS "Setting to true.")
endif()
message(STATUS "HAVE_CONCEPT_IMPLEMENTATION = ${HAVE_CONCEPT_IMPLEMENTATION}")

target_compile_definitions(commons PUBLIC COMMONS_HAS_OPENGL=${CMN_FOUND_OPENGL} COMMONS_PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE}")
if(COMMONS_NO_PYTHON)
    target_compile_definitions(commons PUBLIC COMMONS_NO_PYTHON=true)
else()
    target_compile_definitions(commons PUBLIC COMMONS_NO_PYTHON=false)
endif()

if(CMN_USE_OPENGL2 AND NOT COMMONS_FOR_JS)
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


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(commons PUBLIC atomic pthread m)
endif()

add_dependencies(commons Commons::OpenCV)

set(RESOURCE_COPY_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonts
)
set(RESOURCE_COPY_SINGLE_FILES
)

set(COMMONS_LIBRARY_NAME commons)

message(STATUS "Copying files ${RESOURCE_COPY_SINGLE_FILES}")
foreach(comp ${RESOURCE_COPY_SINGLE_FILES})
    get_filename_component(comp_s ${comp} NAME)
    add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${comp}"
                    "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
    install(FILES ${comp} DESTINATION bin)
endforeach()

foreach(comp ${RESOURCE_COPY_FILES})
    get_filename_component(comp_s ${comp} NAME)
    add_custom_command(TARGET ${COMMONS_LIBRARY_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${comp}"
            "$<TARGET_FILE_DIR:${COMMONS_LIBRARY_NAME}>/${comp_s}")
endforeach()
