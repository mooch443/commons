set(RESOURCES_ADDED "")

include(${CMAKE_CURRENT_SOURCE_DIR}/CommonsSources.cmake)

set(HDRS ${COMMONS_PUBLIC_HEADERS})
set(SRCS ${COMMONS_CPP_SOURCES})
set(COMMONS_MODULE_FILES ${COMMONS_MODULE_INTERFACE_UNITS})

if(TREX_ENABLE_SHARED_INTERNAL_LIBS)
    set(COMMONS_LIBRARY_TYPE SHARED)
else()
    set(COMMONS_LIBRARY_TYPE STATIC)
endif()

file(GLOB HASH_HDR ${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map/*.hpp)

message(STATUS "hash_map: ${HASH_HDR} ${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map")

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" AND METAL_LIBRARY)
    set(SRCS ${SRCS} gui/MetalImpl.mm file/ask_for_permission.mm)
    # after add_executable() or target_sources() for your target…
    set_source_files_properties(
        gui/MetalImpl.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
    set_source_files_properties(
        file/ask_for_permission.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
else()
    message(STATUS "Skipping metal implementation")
endif()

if(APPLE)
   set(SRCS ${SRCS} gui/MacProgressBar.mm)
   set_source_files_properties(
        gui/MacProgressBar.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
   message(STATUS "Adding MacProgressBar")
endif()

if(NOT COMMONS_BUILD_HTTPD)
    list(REMOVE_ITEM HDRS http/httpd.h)
    list(REMOVE_ITEM SRCS http/httpd.cpp)
    list(REMOVE_ITEM COMMONS_MODULE_FILES modules/commons.http.cppm)
endif()

if(COMMONS_ENABLE_MODULES)
    if(${CMAKE_VERSION} VERSION_LESS "3.28.0")
        message(FATAL_ERROR "COMMONS_ENABLE_MODULES requires CMake 3.28 or newer.")
    endif()

    if(NOT CMAKE_GENERATOR MATCHES "^(Ninja|Ninja Multi-Config|Visual Studio 17 2022)$")
        message(FATAL_ERROR
            "COMMONS_ENABLE_MODULES is not supported with the '${CMAKE_GENERATOR}' generator. "
            "Current CMake C++ module support requires Ninja, Ninja Multi-Config, or Visual Studio 17 2022. "
            "Use -G Ninja (or disable COMMONS_ENABLE_MODULES)."
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND NOT COMMONS_ENABLE_EXPERIMENTAL_GCC_MODULES)
        message(FATAL_ERROR "COMMONS_ENABLE_MODULES is experimental on GCC. Reconfigure with -DCOMMONS_ENABLE_EXPERIMENTAL_GCC_MODULES=ON or use the legacy header build.")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        message(WARNING "Building commons modules with GCC in experimental mode.")
    endif()
endif()

# to include generated moc headers, since they are generated in binary dir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(commons ${COMMONS_LIBRARY_TYPE} ${SRCS} ${HDRS} ${HASH_HDR})
add_dependencies(commons Commons::LibZIP Commons::LibZ Commons::GLFW Commons::OpenGL cnpy Commons::OpenCV Commons::LibPNG Commons::GLAD)
if(COMMONS_ENABLE_MODULES)
    target_include_directories(commons PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../robin-hood-hashing/src/include>")
else()
    target_include_directories(commons PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include/commons>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../robin-hood-hashing/src/include>")
endif()

if(COMMONS_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(commons PRIVATE TREX_EXPORTS)
    target_compile_definitions(commons PUBLIC COMMONS_IS_SHARED_LIBRARY=1)
else()
    target_compile_definitions(commons PUBLIC COMMONS_IS_SHARED_LIBRARY=0)
endif()

if(APPLE AND TREX_ENABLE_SHARED_INTERNAL_LIBS)
    set_target_properties(commons PROPERTIES
        CXX_VISIBILITY_PRESET default
        C_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
    target_compile_options(commons PRIVATE -fvisibility=default)
    target_link_options(commons PRIVATE "-Wl,-undefined,error")
endif()

if(COMMONS_ENABLE_MODULES)
    target_sources(commons
        PUBLIC
            FILE_SET commons_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES ${COMMONS_MODULE_FILES}
    )
endif()

foreach(FILE ${HASH_HDR})
    source_group("hash_map" FILES "${FILE}")
endforeach()

if(NOT COMMONS_ENABLE_MODULES)
    install(FILES ${HASH_HDR} DESTINATION include/commons COMPONENT devel)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../robin-hood-hashing/src/include/robin_hood.h" DESTINATION include/commons COMPONENT devel)
endif()

foreach(FILE ${SRCS} ${HDRS} ${COMMONS_MODULE_FILES}) 
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

    # Remove common directory prefix to make the group
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

    # Make sure we are using windows slashes
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    # Group into "Source Files" and "Header Files"
    if ("${FILE}" MATCHES ".*\\.cpp")
       set(GROUP "${GROUP}")
    elseif("${FILE}" MATCHES ".*\\.cppm")
       set(GROUP "${GROUP}")
    elseif("${FILE}" MATCHES ".*\\.h")
       set(GROUP "${GROUP}")
    endif()

    source_group("${GROUP}" FILES "${FILE}")
endforeach()

# Find ICU package
find_package(ICU REQUIRED COMPONENTS uc i18n)

# Output ICU variables for debugging
message(STATUS "Found ICU:")
message(STATUS "  ICU_VERSION: ${ICU_VERSION}")
message(STATUS "  ICU_LIBRARIES: ${ICU_LIBRARIES}")
message(STATUS "  ICU_INCLUDE_DIRS: ${ICU_INCLUDE_DIRS}")

# Add ICU include directories
if(COMMONS_ENABLE_MODULES)
    target_include_directories(commons PRIVATE ${ICU_INCLUDE_DIRS})
else()
    target_include_directories(commons PUBLIC ${ICU_INCLUDE_DIRS})
endif()

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
    PUBLIC glaze::glaze
    PUBLIC ${ICU_LIBRARIES}
    #PUBLIC nlohmann_json::nlohmann_json
)

option(COMMONS_DONT_USE_PCH OFF)
if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
    message("Please consider to switch to CMake 3.16.0")
elseif(NOT COMMONS_DONT_USE_PCH AND NOT COMMONS_ENABLE_MODULES)
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
            if("${osx_dir}" STREQUAL "")
                # Some SDK discovery paths can resolve to an empty version suffix.
                # Fall back to the explicit deployment target if available.
                set(osx_dir "${CMAKE_OSX_DEPLOYMENT_TARGET}")
            endif()
            message(STATUS "OSX Target: ${osx_dir}")
            set(COMMONS_MACOS_VERSION ${osx_dir})
            if(NOT "${osx_dir}" STREQUAL "")
                string(REPLACE "." ";" VERSION_LIST "${osx_dir}")
                list(LENGTH VERSION_LIST VERSION_LIST_LEN)
                if(VERSION_LIST_LEN GREATER 0)
                    list(GET VERSION_LIST 0 COMMONS_MACOS_VERSION_MAJOR)
                endif()
                if(VERSION_LIST_LEN GREATER 1)
                    list(GET VERSION_LIST 1 COMMONS_MACOS_VERSION_MINOR)
                else()
                    set(COMMONS_MACOS_VERSION_MINOR 0)
                endif()
                target_compile_definitions(commons PUBLIC COMMONS_MACOS_VERSION_MAJOR=${COMMONS_MACOS_VERSION_MAJOR})
                target_compile_definitions(commons PUBLIC COMMONS_MACOS_VERSION_MINOR=${COMMONS_MACOS_VERSION_MINOR})
                message(STATUS "OSX Major: ${COMMONS_MACOS_VERSION_MAJOR}, Minor: ${COMMONS_MACOS_VERSION_MINOR}")
            else()
                message(WARNING "Could not determine macOS version from sysroot or deployment target.")
            endif()
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
