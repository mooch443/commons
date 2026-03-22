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

# Find ICU package early so the layer target helpers can use the result.
find_package(ICU REQUIRED COMPONENTS uc i18n)

# Output ICU variables for debugging
message(STATUS "Found ICU:")
message(STATUS "  ICU_VERSION: ${ICU_VERSION}")
message(STATUS "  ICU_LIBRARIES: ${ICU_LIBRARIES}")
message(STATUS "  ICU_INCLUDE_DIRS: ${ICU_INCLUDE_DIRS}")

set(COMMONS_FILE_CPP_SOURCES ${SRCS})
list(FILTER COMMONS_FILE_CPP_SOURCES INCLUDE REGEX "^(file/|misc/(CommandLine|GlobalSettings)\\.cpp$)")
list(REMOVE_ITEM COMMONS_FILE_CPP_SOURCES
    file/Path.cpp
    misc/CommandLine.cpp
    misc/GlobalSettings.cpp
)

set(COMMONS_PROCESSING_CPP_SOURCES ${SRCS})
list(FILTER COMMONS_PROCESSING_CPP_SOURCES INCLUDE REGEX "^(processing/|types\\.cpp$|misc/(PVBlob|PixelTree|ProximityGrid|bid)\\.cpp$)")

set(COMMONS_VIDEO_CPP_SOURCES ${SRCS})
list(FILTER COMMONS_VIDEO_CPP_SOURCES INCLUDE REGEX "^video/")

set(COMMONS_GUI_CPP_SOURCES ${SRCS})
list(FILTER COMMONS_GUI_CPP_SOURCES INCLUDE REGEX "^gui/")
if(COMMONS_BUILD_HTTPD)
    list(APPEND COMMONS_GUI_CPP_SOURCES http/httpd.cpp)
endif()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" AND METAL_LIBRARY)
    list(APPEND COMMONS_FILE_CPP_SOURCES file/ask_for_permission.mm)
endif()

set(COMMONS_MISC_CPP_SOURCES ${SRCS})
list(REMOVE_ITEM COMMONS_MISC_CPP_SOURCES
    ${COMMONS_FILE_CPP_SOURCES}
    ${COMMONS_PROCESSING_CPP_SOURCES}
    ${COMMONS_VIDEO_CPP_SOURCES}
    ${COMMONS_GUI_CPP_SOURCES}
)

function(commons_configure_target target)
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../flat_hash_map>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../robin-hood-hashing/src/include>"
        "$<BUILD_INTERFACE:${glaze_PATH}/include>"
        "$<TARGET_PROPERTY:cnpy,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:Commons::OpenCV,INTERFACE_INCLUDE_DIRECTORIES>")

    if(NOT COMMONS_ENABLE_MODULES)
        target_include_directories(${target} PUBLIC "$<INSTALL_INTERFACE:include/commons>")
    endif()

    if(COMMONS_ENABLE_MODULES)
        target_include_directories(${target} PRIVATE ${ICU_INCLUDE_DIRS})
    else()
        target_include_directories(${target} PUBLIC ${ICU_INCLUDE_DIRS})
    endif()

    if(COMMONS_LIBRARY_TYPE STREQUAL "SHARED")
        target_compile_definitions(${target} PRIVATE TREX_EXPORTS)
        target_compile_definitions(${target} PUBLIC COMMONS_IS_SHARED_LIBRARY=1)
    else()
        target_compile_definitions(${target} PUBLIC COMMONS_IS_SHARED_LIBRARY=0)
    endif()

    if(APPLE AND TREX_ENABLE_SHARED_INTERNAL_LIBS)
        set_target_properties(${target} PROPERTIES
            CXX_VISIBILITY_PRESET default
            C_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF
        )
        target_compile_options(${target} PRIVATE -fvisibility=default)
        target_link_options(${target} PRIVATE "-Wl,-undefined,error")
    endif()

    if(NOT COMMONS_ENABLE_MODULES AND NOT COMMONS_DONT_USE_PCH)
        target_precompile_headers(${target} PUBLIC commons.pc.h)
    endif()
endfunction()

function(commons_link_dependencies target layer)
    target_link_libraries(${target} PUBLIC
        cnpy
        Commons::OpenCV
        Commons::LibPNG
        Commons::LibZIP
        Commons::LibZ
        ${ICU_LIBRARIES}
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_libraries(${target} PUBLIC atomic pthread m)
    endif()

    if(layer STREQUAL "video" OR layer STREQUAL "monolith")
        target_link_libraries(${target} PUBLIC Commons::FFMPEG)
    endif()

    if(layer STREQUAL "gui" OR layer STREQUAL "monolith")
        target_link_libraries(${target} PUBLIC
            Commons::IMGUI
            Commons::GLFW
            Commons::OpenGL
        )
        if(COMMONS_BUILD_HTTPD)
            target_link_libraries(${target} PUBLIC Commons::HTTPD)
        endif()
    endif()
endfunction()

if(COMMONS_ENABLE_MODULES)
    add_library(commons ${COMMONS_LIBRARY_TYPE} ${SRCS} ${HDRS} ${HASH_HDR})
    commons_configure_target(commons)
    commons_link_dependencies(commons monolith)
    target_sources(commons
        PUBLIC
            FILE_SET commons_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES ${COMMONS_MODULE_FILES}
    )
    set(COMMONS_ACTIVE_TARGETS commons)
    set(COMMONS_LIBRARY_NAME commons)
else()
    if(COMMONS_SPLIT_SHARED_LIBS AND NOT TREX_ENABLE_SHARED_INTERNAL_LIBS)
        message(STATUS "COMMONS_SPLIT_SHARED_LIBS is ignored because TREX_ENABLE_SHARED_INTERNAL_LIBS is OFF.")
    endif()

    if(COMMONS_SPLIT_SHARED_LIBS AND TREX_ENABLE_SHARED_INTERNAL_LIBS)
        add_library(commons_misc ${COMMONS_LIBRARY_TYPE} ${COMMONS_MISC_CPP_SOURCES} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons_misc)
        commons_link_dependencies(commons_misc misc)

        add_library(commons_file ${COMMONS_LIBRARY_TYPE} ${COMMONS_FILE_CPP_SOURCES} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons_file)
        commons_link_dependencies(commons_file file)
        target_link_libraries(commons_file PUBLIC commons_misc)

        add_library(commons_processing ${COMMONS_LIBRARY_TYPE} ${COMMONS_PROCESSING_CPP_SOURCES} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons_processing)
        commons_link_dependencies(commons_processing processing)
        target_link_libraries(commons_processing PUBLIC commons_file)

        add_library(commons_video ${COMMONS_LIBRARY_TYPE} ${COMMONS_VIDEO_CPP_SOURCES} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons_video)
        commons_link_dependencies(commons_video video)
        target_link_libraries(commons_video PUBLIC commons_processing)

        add_library(commons_gui ${COMMONS_LIBRARY_TYPE} ${COMMONS_GUI_CPP_SOURCES} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons_gui)
        commons_link_dependencies(commons_gui gui)
        target_link_libraries(commons_gui PUBLIC commons_video)

        add_library(commons INTERFACE)
        target_link_libraries(commons INTERFACE commons_gui)
        set(COMMONS_LIBRARY_NAME commons_gui)
        set(COMMONS_ACTIVE_TARGETS
            commons_misc
            commons_file
            commons_processing
            commons_video
            commons_gui
        )
    else()
        add_library(commons ${COMMONS_LIBRARY_TYPE} ${SRCS} ${HDRS} ${HASH_HDR})
        commons_configure_target(commons)
        commons_link_dependencies(commons monolith)
        set(COMMONS_LIBRARY_NAME commons)
        set(COMMONS_ACTIVE_TARGETS commons)
    endif()
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

option(COMMONS_DONT_USE_PCH OFF)
if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
    message("Please consider to switch to CMake 3.16.0")
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
                foreach(target ${COMMONS_ACTIVE_TARGETS})
                    target_compile_definitions(${target} PUBLIC COMMONS_MACOS_VERSION_MAJOR=${COMMONS_MACOS_VERSION_MAJOR})
                    target_compile_definitions(${target} PUBLIC COMMONS_MACOS_VERSION_MINOR=${COMMONS_MACOS_VERSION_MINOR})
                endforeach()
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
    foreach(target ${COMMONS_ACTIVE_TARGETS})
        target_compile_definitions(${target} PUBLIC HAVE_CONCEPT_IMPLEMENTATION=true)
    endforeach()
    message(STATUS "Setting to true.")
endif()
message(STATUS "HAVE_CONCEPT_IMPLEMENTATION = ${HAVE_CONCEPT_IMPLEMENTATION}")

foreach(target ${COMMONS_ACTIVE_TARGETS})
    target_compile_definitions(${target} PUBLIC COMMONS_HAS_OPENGL=${CMN_FOUND_OPENGL} COMMONS_PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE}")
    if(COMMONS_NO_PYTHON)
        target_compile_definitions(${target} PUBLIC COMMONS_NO_PYTHON=true)
    else()
        target_compile_definitions(${target} PUBLIC COMMONS_NO_PYTHON=false)
    endif()

    if(CMN_USE_OPENGL2 AND NOT COMMONS_FOR_JS)
        target_compile_definitions(${target} PUBLIC CMN_USE_OPENGL2=true)
    else()
        target_compile_definitions(${target} PUBLIC CMN_USE_OPENGL2=false)
    endif()

    if(APPLE)
        target_compile_options(${target} PRIVATE "-DNS_FORMAT_ARGUMENT(A)=" "-D_Nullable_result=_Nullable")
    endif()
endforeach()

set(RESOURCE_COPY_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonts
)
set(RESOURCE_COPY_SINGLE_FILES
)

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
