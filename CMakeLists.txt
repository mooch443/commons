cmake_minimum_required(VERSION 3.0)
if(POLICY CMP0114)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0114 NEW)
endif()

project(commons)

include(ExternalProject)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.12.0")
    include(FetchContent)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules )

message("CXX FLAGS: " ${CMAKE_CXX_FLAGS})

if(APPLE)
    option(COMMONS_WITH_OPENGL OFF)
else()
    option(COMMONS_WITH_OPENGL ON)
endif()
option(COMMONS_BUILD_GLFW ON)
option(COMMONS_BUILD_PNG ON)
option(COMMONS_BUILD_ZIP ON)
option(COMMONS_FOR_JS OFF)
option(COMMONS_BUILD_ZLIB ON)
option(COMMONS_BUILD_OPENCV ON)
option(COMMONS_CONDA_PACKAGE_INSTALL OFF)
option(CMN_USE_OPENGL2 OFF)
option(WITH_FFMPEG "Compile with ffmpeg streaming support" ON)
option(COMMONS_BUILD_HTTPD "Compile with libmicrohttpd (enable web features)" OFF)
option(NO_PUT_TIME "Disables std::put_time usage" OFF)
option(OPENCV_EXTRA_FLAGS "OpenCV extra CMake flags" "")
option(COMMONS_NO_PYTHON "Disable Python support" OFF)
option(COMMONS_ENABLE_CPP20 "Enables CPP20 language standard (if available). Only for GCC and Clang at the moment. Experimental feature." ON)
set(OPENCV_EXTRA_FLAGS "")

if(COMMONS_ENABLE_CPP20)
    set(CMAKE_CXX_STANDARD 20)
    message(STATUS "Using c++20 standard (${CMAKE_CXX_STANDARD}).")

    set(cxx_flag_std_number "")
    if(CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag(-std=c++2a HAVE_FLAG_STD_CXX20)
        if(HAVE_FLAG_STD_CXX20)
            message(STATUS "Using support for C++20")
            set(CMAKE_CXX_STANDARD 20)
            set(cxx_flag_std_number "-std=c++2a")
        endif()

        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${cxx_flag_std_number}")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${cxx_flag_std_number}")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${cxx_flag_std_number}")
    endif()
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
string( TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE )
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;DEBUG;")

if(APPLE)
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
        endif()
    endif()
endif()

if(APPLE AND "${COMMONS_MACOS_VERSION}" VERSION_LESS "10.12")
    message(STATUS "This system does not support MetalKit (detected version: ${COMMONS_MACOS_VERSION}), enabling OpenGL.")
    SET(COMMONS_WITH_OPENGL ON CACHE BOOL "" FORCE)
elseif(APPLE)
    message(STATUS "MetalKit support detected (version: ${COMMONS_MACOS_VERSION}).")
else()
    message(STATUS "Non-Apple systems use OpenGL.")
    SET(COMMONS_WITH_OPENGL ON CACHE BOOL "" FORCE)
endif()

if(NO_PUT_TIME)
    add_definitions(-DNO_PUT_TIME=true)
endif()

set(CONDA_PREFIX $ENV{CONDA_PREFIX} CACHE PATH "CONDA_PREFIX if set during cmake")
if(CONDA_PREFIX)
    message(STATUS "CONDA_PREFIX=${CONDA_PREFIX}")
endif()

if(COMMONS_CONDA_PACKAGE_INSTALL)
    add_definitions(-DCOMMONS_CONDA_PACKAGE_INSTALL=1)
    set(CMAKE_BUILD_RPATH "${CONDA_PREFIX}/lib")
    
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    endif("${isSystemDir}" STREQUAL "-1")
endif()

set(CMAKE_DEBUG_POSTFIX -d)


if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
endif()


if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten") #-pthread 
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -O3 -s FETCH=1 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s FULL_ES3=1 -s USE_GLFW=3 -s LEGACY_GL_EMULATION=0 -s \"BINARYEN_METHOD='native-wasm'\" -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s PTHREAD_POOL_SIZE=4 -s GL_PREINITIALIZED_CONTEXT=1") # 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -pthread")
    set(CMAKE_CONFIGURATION_TYPES "Release" FORCE)
endif()

set(ORIGINAL_CXX_FLAGS ${CMAKE_CXX_FLAGS})


if(WIN32)
    set(BUILD_SHARED_LIBS OFF)
    string(REPLACE " " ";" VERSION_LIST ${CMAKE_GENERATOR})
    list(GET VERSION_LIST 2 VSTD_VERSION_MAJOR)
    
    math (EXPR MSVC_TOOLSET_MAJOR "${MSVC_VERSION}/10")
    message(STATUS "CMAKE_GENERATOR_TOOLSET ${MSVC_TOOLSET_MAJOR} ${CMAKE_GENERATOR} ${VSTD_VERSION_MAJOR}")
    
    #if(${MSVC_TOOLSET_MAJOR} LESS 190)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_ENABLE_ATOMIC_ALIGNMENT_FIX")
    #endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_USE_MATH_DEFINES -DNOMINMAX -DCURL_STATICLIB")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /bigobj")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd /bigobj")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    
    set(GLEW_STATIC ON)
else()
    set(BUILD_SHARED_LIBS OFF)
endif()

message(STATUS "ORIGINAL_CXX_FLAGS=${ORIGINAL_CXX_FLAGS}")

add_library(Commons::X11 INTERFACE IMPORTED GLOBAL)
if(UNIX AND NOT APPLE AND NOT COMMONS_FOR_JS)
    message(STATUS "Finding X11...")
    find_package(X11 REQUIRED)
    target_link_libraries(Commons::X11 INTERFACE X11::X11 ${X11_LIBRARIES} dl)
    
    message(STATUS "X11_LIBRARIES: ${X11_LIBRARIES} ${X11_xcb_FOUND} DIRS:${X11_LIBRARY_DIRS} CFLAGS:${X11_CFLAGS}")
    link_directories(${X11_LIBRARY_DIRS})
    include_directories(${X11_INCLUDE_DIRS})
endif()

add_library(Commons::OpenGL INTERFACE IMPORTED GLOBAL)
set(CMN_FOUND_OPENGL false)
if(COMMONS_FOR_JS)
    target_link_libraries(Commons::OpenGL INTERFACE GL glfw3)
endif()

if((COMMONS_BUILD_GLFW OR UNIX) AND NOT COMMONS_FOR_JS)
    if(WIN32)
        find_package(OpenGL REQUIRED)
        target_link_libraries(Commons::OpenGL INTERFACE OpenGL::GL)
        message(STATUS "OpenGL library ${OPENGL_LIBRARY}")
        include_directories(${OPENGL_INCLUDE_DIR})
        set(CMN_FOUND_OPENGL true)

        target_include_directories(Commons::OpenGL INTERFACE
          "$<BUILD_INTERFACE:${OPENGL_INCLUDE_DIR}>"
          #"$<INSTALL_INTERFACE:${INSTALL_DIR}/lib>"
        )
        set_target_properties(Commons::OpenGL PROPERTIES
          IMPORTED_LOCATION "${OPENGL_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${OPENGL_INCLUDE_DIR}"
          #INTERFACE_LINK_DIRECTORIES "${INSTALL_DIR}/lib"
        )
    elseif(UNIX AND NOT APPLE)
        find_package(OpenGL REQUIRED)
        target_link_libraries(Commons::OpenGL INTERFACE OpenGL::GL)
        SET(OPENGL_LIBRARY OpenGL::GL)
        message(STATUS "OpenGL library ${OPENGL_LIBRARY}")
        include_directories(${OPENGL_INCLUDE_DIR})
        set(CMN_FOUND_OPENGL true)
    else()
        if(COMMONS_WITH_OPENGL)
            find_library(OPENGL_LIBRARY OpenGL)
            target_link_libraries(Commons::OpenGL INTERFACE ${OPENGL_LIBRARY})
            set(CMN_FOUND_OPENGL true)
        else()
            set(CMN_FOUND_OPENGL false)
        endif()
    endif()
elseif(NOT COMMONS_FOR_JS)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Commons::OpenGL INTERFACE OpenGL::GL)
    message(STATUS "OpenGL library ${OPENGL_LIBRARY}")
    include_directories(${OPENGL_INCLUDE_DIR})
    set(CMN_FOUND_OPENGL true)
endif()

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-undefined,error -framework IOKit -framework CoreFoundation -framework CoreGraphics -framework CoreVideo -framework OpenCL -framework Cocoa -framework AppKit -framework AVKit -framework AVFoundation -framework CoreMedia -framework QuartzCore -framework Accelerate")
    
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    
    if(CMAKE_OSX_SYSROOT AND NOT ${CMAKE_OSX_SYSROOT} STREQUAL "")
        find_library(METALKIT_LIBRARY MetalKit ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
        find_library(METAL_LIBRARY Metal ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
        message(STATUS "CMAKE_OSX_SYSROOT: ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks NO_DEFAULT_PATH")
    else()
        find_library(METALKIT_LIBRARY MetalKit)
        find_library(METAL_LIBRARY Metal)
        message(STATUS "Trying to find Metal in default paths.")
    endif()
    
    find_library(QUARTZ_LIBRARY QuartzCore)
    if(COMMONS_WITH_OPENGL)
        set(EXTRA_LIBS Commons::GLAD Commons::IMGUI Commons::OpenGL)
    else()
        set(EXTRA_LIBS Commons::IMGUI)
    endif()
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${QUARTZ_LIBRARY}
    )
    
    if(NOT METAL_LIBRARY)
        message(STATUS "Did not find metal")
        
    else()
        message(STATUS "Did find metal")
        set(EXTRA_LIBS ${EXTRA_LIBS}
            ${METALKIT_LIBRARY}
            ${METAL_LIBRARY}
        )
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal")
    endif()
    
    message(STATUS "Mac extra libs: ${EXTRA_LIBS}")
    target_link_libraries(Commons::OpenGL INTERFACE ${EXTRA_LIBS})

else()
    if(COMMONS_WITH_OPENGL)
        set(EXTRA_LIBS Commons::GLAD Commons::IMGUI Commons::OpenGL)
    else()
        set(EXTRA_LIBS Commons::IMGUI)
    endif()
endif(APPLE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
else()

set(COMMONS_CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
set(COMMONS_CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DTT_DEBUG_ENABLED")

# Set output paths
set(PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
endif()


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(COMMONS_CONDA_PACKAGE_INSTALL)
    set(COMMONS_CMAKE_BUILD_TYPE Release)
else()
    if(APPLE)
        set(COMMONS_CMAKE_BUILD_TYPE $<$<CONFIG:Debug>:Release>$<$<CONFIG:Release>:Debug>)
    else()
        set(COMMONS_CMAKE_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,Release>)
    endif()
endif()

if(COMMONS_FOR_JS)
    set(COMMONS_CMAKE_BUILD_TYPE Release)
endif()

#if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message(STATUS "Not adding any options.")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    #set(C_SANITIZE_OPTIONS "-fsanitize=thread,signed-integer-overflow,null,alignment,undefined,bounds -fno-sanitize-recover=null -fsanitize-trap=alignment,undefined,signed-integer-overflow")
    set(C_SANITIZE_OPTIONS "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-stack-address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${C_SANITIZE_OPTIONS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g ${C_SANITIZE_OPTIONS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic -Wall -Wextra -pedantic")
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=return-stack-address")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${C_SANITIZE_OPTIONS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -g ${C_SANITIZE_OPTIONS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-c++98-compat-pedantic")
    message(STATUS "hidden visibility in main project ${CMAKE_CXX_FLAGS}")
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-nullability-extension")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-nullability-extension")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${C_SANITIZE_OPTIONS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${C_SANITIZE_OPTIONS}")
elseif(MSVC)
else()
    message(STATUS "***** compiler ID ${CMAKE_CXX_COMPILER_ID}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-local-addr")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wno-long-long -ggdb -DOM_DEBUG -O0 -no-pie")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0 -no-pie")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -ggdb -no-pie")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie -fPIC -Werror=return-local-addr")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -ggdb")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wno-long-long -ggdb -DOM_DEBUG -O0")
endif()

if(XCODE)
    if(COMMONS_ENABLE_CPP20)
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")
    else()    
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17")
    endif()
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
    
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_RETURN_TYPE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_NEWLINE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VALUE "YES")
    #set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_SIGN_COMPARE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_INITIALIZER_NOT_FULLY_BRACKETED "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNINITIALIZED_AUTOS "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_BOOL_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_EMPTY_BODY "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_ENUM_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_INT_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_CONSTANT_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_NON_VIRTUAL_DESTRUCTOR "YES")
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    FINDING      P Y T H O N
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#if(NOT COMMONS_NO_PYTHON)
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    #if(CONDA_PREFIX AND NOT Python_ROOT_DIR)
        message(STATUS "Using conda prefix = ${CONDA_PREFIX} to find Python.")
        set(Python_ROOT_DIR "${CONDA_PREFIX}")
        set(Python_DIR "${CONDA_PREFIX}")
        message(STATUS "Set conda prefix ${Python_DIR} (exec ${Python_EXECUTABLE} vs. ${PYTHON_EXECUTABLE})")
    #endif()

    if(CONDA_PREFIX)
        set(Python_ROOT_DIR ${CONDA_PREFIX})
    endif()
    set(Python_FIND_STRATEGY LOCATION)
    unset(Python_USE_STATIC_LIBS)
    find_package(Python 3.6 COMPONENTS Interpreter Development REQUIRED)

    message(STATUS "After find PYTHON: ${Python_FOUND}")
    message(STATUS "PYTHON: ${Python_LIBRARIES}")
    message(STATUS "PYTHON: ${Python_EXECUTABLE}")

    set(PYTHON_EXECUTABLE "${Python_EXECUTABLE}")
    set(PYTHON_LIBRARY "${Python_LIBRARIES}")
endif()

set(COMMONS_subproject_CMAKE_ARGS "")

SET(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
    -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
    -DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
    -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
    -DCMAKE_C_FLAGS_RELEASE:STRING=${CMAKE_C_FLAGS_RELEASE}
    -DCMAKE_C_FLAGS_RELWITHDEBINFO:STRING=${CMAKE_C_FLAGS_RELWITHDEBINFO}
    -DCMAKE_C_FLAGS_DEBUG:STRING=${CMAKE_C_FLAGS_DEBUG}
    #-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_CXX_FLAGS=${ORIGINAL_CXX_FLAGS}
    -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
    -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
    -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    -DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF 
    -DSKIP_INSTALL_FILES=1
    -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
    -DCMAKE_EXE_LINKER_FLAGS_DEBUG=${CMAKE_EXE_LINKER_FLAGS_DEBUG}
    -DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}
    -DPYTHON_INCLUDE_DIR:PATH=${PYTHON_INCLUDE_DIR}
    -DPYTHON_LIBRARY:PATH=${PYTHON_LIBRARY}
    -DPYTHON_EXECUTABLE:FILEPATH=${PYTHON_EXECUTABLE}
    -DPython_ROOT_DIR:FILEPATH=${Python_ROOT_DIR}
    -DPython_FIND_STRATEGY=LOCATION

)

if(APPLE)
    set(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
        -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET = ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "MACOSX_DEPLOYMENT_TARGET = $ENV{MACOSX_DEPLOYMENT_TARGET}")

#if(NOT APPLE AND NOT COMMONS_NO_PYTHON)
#    set(PY_VERSION "3.6")
#    find_package(PythonLibs ${PY_VERSION} REQUIRED)
#    find_package(PythonInterp ${PY_VERSION} REQUIRED)
#    include_directories(${PYTHON_INCLUDE_DIRS})

#    message(STATUS "${PYTHON_INCLUDE_DIRS} ${PYTHON_LIBRARIES} ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")
#    message(STATUS "Using python version ${PYTHON_EXECUTABLE}")
#endif()

if (DEFINED TRACKER_GLOBAL_THREADS)
    ADD_DEFINITIONS(-DTRACKER_GLOBAL_THREADS=${TRACKER_GLOBAL_THREADS})
endif()
#ADD_DEFINITIONS(-DCV_STATIC_ANALYSIS=0)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/src/commons/CMakeModules" )

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      G L A D
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

add_library (Commons::GLAD INTERFACE IMPORTED GLOBAL)
if(COMMONS_WITH_OPENGL AND NOT COMMONS_FOR_JS)
    # glad
    if(CMN_USE_OPENGL2)
        set(GLAD_PROFILE "compatibility")
        set(GLAD_API "gl=2.0")
    else()
        set(GLAD_PROFILE "core")
        set(GLAD_API "gl=3.2")
    endif()
    ExternalProject_Add(gladex PREFIX gladex
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v0.1.33
        CMAKE_ARGS
            ${COMMONS_subproject_CMAKE_ARGS}
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DGLAD_PROFILE=${GLAD_PROFILE}
            -DGLAD_API=${GLAD_API}
            -DGLAD_INSTALL=ON
            -DGLAD_REPRODUCIBLE=ON
        UPDATE_COMMAND ""
    )

    ExternalProject_Get_Property(gladex INSTALL_DIR)
    include_directories(${INSTALL_DIR}/include)
    link_directories(${INSTALL_DIR}/lib)
    set(GLAD_INSTALL_DIR ${INSTALL_DIR})
    message(STATUS "glad: ${INSTALL_DIR}")

    file(MAKE_DIRECTORY ${INSTALL_DIR}/include)
    target_link_libraries(Commons::GLAD INTERFACE glad)
    target_include_directories(Commons::GLAD INTERFACE
      "$<BUILD_INTERFACE:${INSTALL_DIR}/include>"
      "$<INSTALL_INTERFACE:${INSTALL_DIR}/lib>"
    )
    set_target_properties(Commons::GLAD PROPERTIES
      IMPORTED_LOCATION "glad"
      INTERFACE_INCLUDE_DIRECTORIES "${INSTALL_DIR}/include"
      INTERFACE_LINK_DIRECTORIES "${INSTALL_DIR}/lib"
    )
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      G L F W
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

add_library (Commons::GLFW INTERFACE IMPORTED GLOBAL)

if(NOT COMMONS_BUILD_GLFW OR COMMONS_FOR_JS)
    find_package(GLFW)
    if(NOT GLFW_FOUND)
        message(STATUS "Cannot find GLFW. Trying to build it instead.")
        set(COMMONS_BUILD_GLFW ON CACHE BOOL "" FORCE)
    else()
        target_link_libraries (Commons::GLFW INTERFACE ${GLFW_LIBRARY})
        include_directories(${GLFW_INCLUDE_DIR})
        
        get_filename_component(GLFW_DIR ${GLFW_LIBRARY} DIRECTORY)
        get_filename_component(GLFW_LIBRARY ${GLFW_LIBRARY} NAME)
        message(STATUS "Linking GLFW: ${GLFW_LIBRARY} includes: ${GLFW_INCLUDE_DIR} dir: ${GLFW_DIR}")
        
        target_link_directories(Commons::GLFW INTERFACE ${GLFW_DIR})

        set_target_properties(Commons::GLFW PROPERTIES
          IMPORTED_LOCATION "${GLFW_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
        )
    endif()
endif()

if(COMMONS_BUILD_GLFW  AND NOT COMMONS_FOR_JS)
    set(GLFW_add_flags "")
    if(WIN32)
        set(GLFW_add_flags ${GLFW_add_flags} -DCMAKE_CXX_FLAGS_RELEASE="/MT" -DCMAKE_CXX_FLAGS_DEBUG="/MTd" -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF)
    endif()

    set(GLFW_CXX_FLAGS "${ORIGINAL_CXX_FLAGS}")
    set(GLFW_C_FLAGS "${CMAKE_C_FLAGS}")

    if(APPLE)
        set(GLFW_CXX_FLAGS "${GLFW_CXX_FLAGS} -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")
        set(GLFW_C_FLAGS "${GLFW_C_FLAGS} -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")
    endif()

    set(GLFW_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/glfw)
    ExternalProject_Add(glfw PREFIX glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
        CMAKE_ARGS
            ${COMMONS_subproject_CMAKE_ARGS}
            "-DCMAKE_CXX_FLAGS:STRING=${GLFW_CXX_FLAGS}"
            "-DCMAKE_C_FLAGS:STRING=${GLFW_C_FLAGS}"
            "-DCMAKE_INSTALL_PREFIX=${GLFW_INSTALL_DIR}"
            "-DGLFW_BUILD_EXAMPLES=OFF"
            "-DGLFW_BUILD_TESTS=OFF"
            "-DGLFW_BUILD_DOCS=OFF"
            ${GLFW_add_flags}
        UPDATE_COMMAND ""
    )
    
    set(GLFW_INCLUDE_DIR ${GLFW_INSTALL_DIR}/include)
    link_directories(${GLFW_INSTALL_DIR}/lib)
    message(STATUS "GLFW LIBRARIES install:${GLFW_INSTALL_DIR}")
    message(STATUS "glfw include: ${GLFW_INCLUDE_DIR}")

    if(APPLE)
        # make directories to avoid warning messages in Xcode
        file(MAKE_DIRECTORY "${GLFW_INSTALL_DIR}/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${GLFW_INSTALL_DIR}/lib/Release")
        file(MAKE_DIRECTORY "${GLFW_INSTALL_DIR}/lib/Debug")
    endif()

    include_directories(${GLFW_INCLUDE_DIR})
    target_link_libraries (Commons::GLFW INTERFACE glfw3 Commons::OpenGL)
    target_include_directories(Commons::GLFW INTERFACE
      "$<BUILD_INTERFACE:${GLFW_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:${GLFW_INSTALL_DIR}/lib>"
    )
    set_target_properties(Commons::GLFW PROPERTIES
      IMPORTED_LOCATION "glfw3"
      INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INSTALL_DIR}/include"
      INTERFACE_LINK_DIRECTORIES "${GLFW_INSTALL_DIR}/lib"
    )

    add_dependencies(Commons::GLFW glfw Commons::OpenGL Commons::GLAD)

    target_link_libraries(Commons::GLFW INTERFACE Commons::GLAD)

    # this is circumventing some weird CMake shuffeling of dependencies. these libraries always have to go last
    # at least once, but multiple times if necessary.
    if(UNIX AND NOT APPLE)
        target_link_libraries(Commons::GLFW INTERFACE Commons::X11 pthread)
    endif()
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      I M G U I
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set (imgui_path ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
message(STATUS "imgui: ${imgui_path}")
file(GLOB imgui_sources ${imgui_path}/*.cpp ${imgui_path}/*.h
    ${imgui_path}/backends/imgui_impl_glfw*.cpp ${imgui_path}/backends/imgui_impl_glfw*.h
)

if(COMMONS_WITH_OPENGL)
    if(NOT COMMONS_FOR_JS)
        set(imgui_sources ${imgui_sources} ${imgui_path}/backends/imgui_impl_opengl2.cpp ${imgui_path}/backends/imgui_impl_opengl2.h)
    endif() 
    if(NOT CMN_USE_OPENGL2 OR COMMONS_FOR_JS)
        set(imgui_sources ${imgui_sources} ${imgui_path}/backends/imgui_impl_opengl3.cpp ${imgui_path}/backends/imgui_impl_opengl3.h)
    endif()
endif()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" AND METAL_LIBRARY)
    set(imgui_sources ${imgui_sources} ${imgui_path}/backends/imgui_impl_metal.mm ${imgui_path}/backends/imgui_impl_metal.h)
endif()

add_library(imgui STATIC ${imgui_sources})
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_definitions(imgui PUBLIC TREX_ENABLE_EXPERIMENTAL_BLUR=1)
endif()

include_directories(${imgui_path})

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    #set_property (TARGET imgui APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")
endif()

add_library (Commons::IMGUI INTERFACE IMPORTED GLOBAL)

target_link_libraries(Commons::IMGUI INTERFACE imgui Commons::GLFW)
target_include_directories(Commons::IMGUI INTERFACE
  "$<BUILD_INTERFACE:${INSTALL_DIR}/include>"
  "$<INSTALL_INTERFACE:${INSTALL_DIR}/lib>"
)
set_target_properties(Commons::IMGUI PROPERTIES
  IMPORTED_LOCATION "imgui"
  INTERFACE_INCLUDE_DIRECTORIES "${imgui_path}"
  #INTERFACE_LINK_DIRECTORIES "${imgui_path}/lib"
)

add_dependencies(imgui Commons::GLFW)

if(COMMONS_WITH_OPENGL)    
    message(STATUS "Commons with OpenGL -----> linking GLAD")
    target_compile_definitions(Commons::IMGUI INTERFACE IMGUI_IMPL_OPENGL_LOADER_GLAD=true)
    target_link_libraries(Commons::IMGUI INTERFACE Commons::GLAD)
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    FINDING      F F M P E G
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library (Commons::FFMPEG INTERFACE IMPORTED GLOBAL)

if(WITH_FFMPEG)
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_library(AVCODEC_LIBRARY avcodec)

    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
    find_library(AVFORMAT_LIBRARY avformat)

    find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
    find_library(AVUTIL_LIBRARY avutil)

    find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
    find_library(AVDEVICE_LIBRARY avdevice)

    #find_path(LIBSWSSCALE_INCLUDE_DIR libswscale/swsscale.h)
    find_library(LIBSWSSCALE_LIBRARY swscale)

    
    get_filename_component(FFMPEG_DIR "${AVDEVICE_LIBRARY}" DIRECTORY)
    message(STATUS "AVDEVICE_INCLUDE_DIR = ${AVDEVICE_INCLUDE_DIR}/ vs. ${FFMPEG_DIR}")

#
    set(OPENCV_EXTRA_FLAGS ${OPENCV_EXTRA_FLAGS} 
        -DFFMPEG_DIR:PATH=${FFMPEG_DIR}
        -DFFMPEG_LIB_DIR:PATH=${FFMPEG_DIR}
        -DFFMPEG_INCLUDE_DIRS=${AVDEVICE_INCLUDE_DIR}
        -DOPENCV_FFMPEG_USE_FIND_PACKAGE=OFF
        )

    if(NOT AVCODEC_LIBRARY OR NOT AVFORMAT_LIBRARY OR NOT AVDEVICE_LIBRARY OR NOT LIBSWSSCALE_LIBRARY)
        message(STATUS "Cannot find the FFMPEG libraries (WITH_FFMPEG=ON) avformat:${AVFORMAT_LIBRARY} avdevice:${AVDEVICE_LIBRARY} swsscale:${LIBSWSSCALE_LIBRARY}")
        message(STATUS "Disabling FFMPEG.")
        set(WITH_FFMPEG OFF)
    else()
        set(FFMPEG_INCLUDE_DIRS ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR})
        set(FFMPEG_LIBRARIES ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${LIBSWSSCALE_LIBRARY})
        message(STATUS "Found ffmpeg: ${FFMPEG_LIBRARIES}")
        target_link_libraries(Commons::FFMPEG INTERFACE ${FFMPEG_LIBRARIES})
        include_directories(${FFMPEG_INCLUDE_DIRS})

        file(MAKE_DIRECTORY ${GLFW_INCLUDE_DIR})
        target_include_directories(Commons::FFMPEG INTERFACE
          "$<BUILD_INTERFACE:${GLFW_INCLUDE_DIR}>"
          "$<INSTALL_INTERFACE:${GLFW_INSTALL_DIR}/lib>"
        )
        set_target_properties(Commons::FFMPEG PROPERTIES
          IMPORTED_LOCATION "${FFMPEG_LIBRARIES}"
          INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
          #INTERFACE_LINK_DIRECTORIES "${GLFW_INSTALL_DIR}/lib"
        )
    endif()
endif()


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      P Y B I N D 1 1
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
set(pybind11_external OFF)

if(${CMAKE_VERSION} VERSION_LESS "3.11.0" AND NOT COMMONS_NO_PYTHON)
    set(pybind11_install_dir ${CMAKE_BINARY_DIR}/pybind11)
    ExternalProject_Add (pybind11
        PREFIX ${pybind11_install_dir}
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DPYBIND11_TEST=OFF
            -DPYBIND11_INSTALL=ON
            ${COMMONS_subproject_CMAKE_ARGS}

        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.10.0 #99ef2b846742e2298b97904e5be43f5ea5d71d8c
        UPDATE_COMMAND ""
    )
    ExternalProject_Get_Property(pybind11 INSTALL_DIR)

    set(pybind11_external ON)
    set(pybind11_LIBRARY ${PYTHON_LIBRARIES} )
    set(pybind11_LIBRARY pybind11::pybind11 pybind11::embed pybind11::module )
    include_directories(${pybind11_install_dir}/include)
    include_directories(${PYTHON_INCLUDE_DIRS})
    message(STATUS "pybind11 external_project ${PYTHON_INCLUDE_DIRS} ${INSTALL_DIR} ${pybind11_LIBRARY}")

elseif(NOT COMMONS_NO_PYTHON)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.10.0
    )

    FetchContent_GetProperties(pybind11)
    if(NOT pybind11_POPULATED)
        FetchContent_Populate(pybind11)
        add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
    endif()

    message(STATUS "PYTHON VARS: lib:${PYTHON_LIBRARY} inc:${PYTHON_INCLUDE_DIR} exec:${PYTHON_EXECUTABLE}")
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    PREPARING for libpng by seeing whether
#    we can link to a previously existing one
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library (Commons::LibPNG INTERFACE IMPORTED GLOBAL)

if(NOT COMMONS_BUILD_PNG AND NOT ${COMMONS_BUILD_PNG})
    find_package(PNG)
    
    if(NOT PNG_FOUND)
        message(STATUS "Did not find PNG. Trying to build it.")
        set(COMMONS_BUILD_PNG ON CACHE BOOL "" FORCE)
        
    else()
        include_directories(${PNG_INCLUDE_DIR})
        target_link_libraries (Commons::LibPNG INTERFACE ${PNG_LIBRARIES})
        target_include_directories(Commons::LibPNG INTERFACE
          "$<BUILD_INTERFACE:${PNG_INCLUDE_DIR}>"
          "$<INSTALL_INTERFACE:${PNG_LIBRARIES}>"
        )
        set_target_properties(Commons::LibPNG PROPERTIES
          IMPORTED_LOCATION "${PNG_LIBRARIES}"
          INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIR}"
          #INTERFACE_LINK_DIRECTORIES "${png_install_dir}/lib"
        )
    
        message(STATUS "png: ${PNG_LIBRARIES} ${PNG_INCLUDE_DIR}")
    endif()
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      L I B Z
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library (Commons::LibZ INTERFACE IMPORTED GLOBAL)
find_package(ZLIB)
if((NOT ${COMMONS_BUILD_ZLIB}) AND ZLIB_FOUND AND ZLIB_VERSION_STRING VERSION_GREATER 1.2.10)
    message(STATUS "Found zlib here: ${ZLIB_LIBRARY}")
    set(libzip_zlib_lib "${ZLIB_LIBRARY}")
    
    target_link_libraries (Commons::LibZ INTERFACE ${ZLIB_LIBRARY})
    unset(ZLIB_LIBRARY)
    
else()
    message(STATUS "Building libZ and libZIP")
    set(COMMONS_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(COMMONS_BUILD_ZIP ON CACHE BOOL "" FORCE)
    set(ZLIB_LIBRARY "z")
    set(zlib_install_dir "${CMAKE_BINARY_DIR}/zlib")
    
    if(WIN32)
        link_directories(${zlib_install_dir}/lib/)
        set(ZLIB_LIBRARY "${zlib_install_dir}/lib/zlibstatic$<IF:$<CONFIG:Debug>,d,>.lib")
        set(libzip_zlib_lib "${zlib_install_dir}/lib/zlibstatic.lib")
        set(COMMONS_subproject_CMAKE_ARGS1 ${COMMONS_subproject_CMAKE_ARGS}
            -DCMAKE_STATIC_LINKER_FLAGS_INIT="/LIBPATH:\"${zlib_install_dir}/lib\"")
    else()
        set(COMMONS_subproject_CMAKE_ARGS1 ${COMMONS_subproject_CMAKE_ARGS})
    endif()
    
    EXTERNALPROJECT_ADD(Z_LIB
        PREFIX ${zlib_install_dir}
        GIT_REPOSITORY https://github.com/mooch443/zlib.git
        GIT_TAG master
        UPDATE_COMMAND ""

        INSTALL_DIR ${zlib_install_dir}
        CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS1} -DCMAKE_DEBUG_POSTFIX=d -DBUILD_STATIC_AND_SHARED=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=${zlib_install_dir}
        -DBUILD_STATIC_AND_SHARED=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF
    )
    
    target_link_libraries (Commons::LibZ INTERFACE ${ZLIB_LIBRARY})

    set(ZLIB_INCLUDE_DIR ${zlib_install_dir}/include)
    include_directories(${ZLIB_INCLUDE_DIR})
    link_directories(${zlib_install_dir}/lib)

    message(STATUS "zlib install dir ${zlib_install_dir}")
    message(STATUS "ZLIB include dir ${ZLIB_INCLUDE_DIR}")
    message(STATUS "ZLIB_LIBRARY = ${ZLIB_LIBRARY} ${ZLIB_VERSION_STRING}")

    if(APPLE)
        # make directories to avoid warning messages in Xcode
        file(MAKE_DIRECTORY "${zlib_install_dir}")
        file(MAKE_DIRECTORY "${zlib_install_dir}/lib")
        file(MAKE_DIRECTORY "${zlib_install_dir}/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${zlib_install_dir}/lib/Release")
        file(MAKE_DIRECTORY "${zlib_install_dir}/lib/Debug")
    endif()

    file(MAKE_DIRECTORY ${ZLIB_INCLUDE_DIR})
    target_include_directories(Commons::LibZ INTERFACE
      "$<BUILD_INTERFACE:${ZLIB_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:${zlib_install_dir}/lib>"
    )
    set_target_properties(Commons::LibZ PROPERTIES
      IMPORTED_LOCATION "${ZLIB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
      INTERFACE_LINK_DIRECTORIES "${zlib_install_dir}/lib"
    )
    
    unset(ZLIB_LIBRARY)
    unset(ZLIB_INCLUDE_DIR)
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      L I B Z I P
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library (Commons::LibZIP INTERFACE IMPORTED GLOBAL)

if(NOT COMMONS_BUILD_ZIP)
    find_package(Zip)
    
    if(NOT Zip_FOUND)
        message(STATUS "Did not find ZIP. Trying to build it.")
        set(COMMONS_BUILD_ZIP ON CACHE BOOL "" FORCE)
        
    else()
        include_directories(${ZIP_INCLUDE_DIR})
        
        target_link_libraries (Commons::LibZIP INTERFACE ${ZIP_LIBRARIES} Commons::LibZ)
        message(STATUS "zip: ${ZIP_LIBRARIES} ${ZIP_INCLUDE_DIR}")
    endif()
endif()

if(COMMONS_BUILD_ZLIB)
    if(WIN32)
        set(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
            -DZLIB_INCLUDE_DIR:PATH=${zlib_install_dir}/include
            -DZLIB_LIBRARY_DEBUG:FILEPATH=${zlib_install_dir}/lib/zlibstaticd.lib
            -DZLIB_LIBRARY_RELEASE:FILEPATH=${zlib_install_dir}/lib/zlibstatic.lib
            -DZLIB_LIBRARY_RELWITHDEBINFO:FILEPATH=${zlib_install_dir}/lib/zlibstatic.lib
        )
        
    else()
        set(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
            -DZLIB_INCLUDE_DIR:PATH=${zlib_install_dir}/include
            -DZLIB_LIBRARY_RELEASE:FILEPATH=${zlib_install_dir}/lib/libz.a
            -DZLIB_LIBRARY_DEBUG:FILEPATH=${zlib_install_dir}/lib/libzd.a
            -DZLIB_LIBRARY_RELWITHDEBINFO:FILEPATH=${zlib_install_dir}/lib/libz.a
        )
    endif()
endif()

if(COMMONS_BUILD_ZIP)
    set(libzip_install_dir "${CMAKE_BINARY_DIR}/libzip")
    set(LIBZIP_LIBRARY "${libzip_install_dir}/lib/zip$<IF:$<CONFIG:Debug>,d,>.lib")
    
    if(NOT WIN32)
        set(LIBZIP_LIBRARY "zip$<IF:$<CONFIG:Debug>,d,>")
    endif()

    if(EMSCRIPTEN)
        set(ZIP_PATCH patch -N -s -d ${libzip_install_dir}/src/libzip -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/libzip_emscripten.diff || echo 1)
    else()
        set(ZIP_PATCH "")
    endif()
    
    ExternalProject_Add(libzip
        PREFIX ${libzip_install_dir}
        DEPENDS Commons::LibZ
        GIT_REPOSITORY https://github.com/nih-at/libzip.git
        GIT_TAG v1.9.2
        UPDATE_COMMAND ""
        PATCH_COMMAND "${ZIP_PATCH}"
        CMAKE_ARGS
            ${libzip_configure}
            -DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}
            ${COMMONS_subproject_CMAKE_ARGS}
            -DENABLE_BZIP2=OFF
            -DBUILD_REGRESS=OFF
            -DBUILD_EXAMPLES=OFF
            -DBUILD_DOC=OFF
            -DBUILD_TOOLS=OFF
            -DENABLE_GNUTLS:BOOL=OFF
            -DENABLE_OPENSSL:BOOL=OFF
            -DENABLE_LZMA:BOOL=OFF
            -DENABLE_MBEDTLS:BOOL=OFF
            -DENABLE_COMMONCRYPTO:BOOL=OFF
            -DENABLE_ZSTD:BOOL=OFF
            -DCMAKE_DEBUG_POSTFIX=d
    )
    
    if(COMMONS_BUILD_ZLIB)
        add_dependencies(libzip Z_LIB)
    endif()

    set(LIBZIP_INCLUDE_DIR "${libzip_install_dir}/include")
    
    include_directories(${LIBZIP_INCLUDE_DIR})
    link_directories(${libzip_install_dir}/lib)
    message(STATUS "ZIP: ${LIBZIP_LIBRARY}")
    message(STATUS "include libzip ${LIBZIP_INCLUDE_DIR} install: ${libzip_install_dir}")
    target_link_libraries (Commons::LibZIP INTERFACE ${LIBZIP_LIBRARY})

    file(MAKE_DIRECTORY ${LIBZIP_INCLUDE_DIR})
    if(APPLE)
        # make directories to avoid warning messages in Xcode
        file(MAKE_DIRECTORY "${libzip_install_dir}/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${libzip_install_dir}/lib/Release")
        file(MAKE_DIRECTORY "${libzip_install_dir}/lib/Debug")
    endif()

    target_include_directories(Commons::LibZIP INTERFACE
      "$<BUILD_INTERFACE:${LIBZIP_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:${libzip_install_dir}/lib>"
    )
    set_target_properties(Commons::LibZIP PROPERTIES
      IMPORTED_LOCATION "${LIBZIP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIR}"
      INTERFACE_LINK_DIRECTORIES "${libzip_install_dir}/lib"
    )

    add_dependencies(Commons::LibZIP Commons::LibZ libzip)
    unset(LIBZIP_LIBRARY)
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      L I B P N G
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

if(${COMMONS_BUILD_PNG})
    message(STATUS "Commons will build PNG.")
    set(png_install_dir "${CMAKE_BINARY_DIR}/libpng")
    set(PNG_LIBRARY "png")

    message(STATUS "PNG_LIBRARY = ${PNG_LIBRARY}")
    message(STATUS "COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}")

    # have to apply a patch solely for M1 builds
    # https://github.com/glennrp/libpng/issues/372
    if(APPLE)
        set(PNG_PATCH patch -N -s -d ${png_install_dir}/src/libpng_custom -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/lib_png_m1patch.diff || echo 1)
    else()
        set(PNG_PATCH "")
    endif()

    #URL ftp://ftp.simplesystems.org/pub/libpng/png/src/history/libpng15/libpng-1.5.10.tar.gz
    #URL_MD5 9e5d864bce8f06751bbd99962ecf4aad
    EXTERNALPROJECT_ADD(libpng_custom
        DEPENDS Commons::LibZ Commons::LibZIP
        PREFIX ${png_install_dir}
        UPDATE_COMMAND ""
        #PATCH_COMMAND ${PNG_PATCH}

        GIT_REPOSITORY  https://github.com/glennrp/libpng.git
        GIT_TAG e56f79c0c6d94f5d727fc147d4487038e188cdf2

        INSTALL_DIR ${png_install_dir}
        CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS} -DCMAKE_PREFIX_PATH=${zlib_install_dir} # to find zlib
        -DPNG_TESTS=OFF
        -DPNG_SHARED=OFF
    )

    include_directories(${png_install_dir}/include)
    link_directories(${png_install_dir}/lib)
    message(STATUS "libpng dir ${png_install_dir}/lib")
    message(STATUS "png library ${PNG_LIBRARY}")

    if(WIN32)
        set(PNG_LIBRARY "${png_install_dir}/lib/libpng16_static$<IF:$<CONFIG:Debug>,d,>.lib")
    else()
        set(PNG_LIBRARY "${png_install_dir}/lib/libpng16$<IF:$<CONFIG:Debug>,d,>.a")
    endif()
    
    target_link_libraries (Commons::LibPNG INTERFACE ${PNG_LIBRARY} Commons::LibZ Commons::LibZIP)
    add_dependencies(Commons::LibPNG ${PNG_LIBRARY} libpng_custom Commons::LibZ Commons::LibZIP)

    file(MAKE_DIRECTORY "${png_install_dir}/include")
    
    if(APPLE)
        # make directories to avoid warning messages in Xcode
        file(MAKE_DIRECTORY "${png_install_dir}/lib")
        file(MAKE_DIRECTORY "${png_install_dir}/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${png_install_dir}/lib/Release")
        file(MAKE_DIRECTORY "${png_install_dir}/lib/Debug")
    endif()

    target_include_directories(Commons::LibPNG INTERFACE
      "$<BUILD_INTERFACE:${png_install_dir}/include>"
      "$<INSTALL_INTERFACE:${png_install_dir}/lib>"
    )
    set_target_properties(Commons::LibPNG PROPERTIES
      IMPORTED_LOCATION "${PNG_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${png_install_dir}/include"
      INTERFACE_LINK_DIRECTORIES "${png_install_dir}/lib"
    )
    
    unset(PNG_LIBRARY)

    if(WIN32)
       set(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
            -DPNG_LIBRARY_RELEASE:FILEPATH=${png_install_dir}/lib/libpng16_static.lib
            -DPNG_LIBRARY_DEBUG:FILEPATH=${png_install_dir}/lib/libpng16_staticd.lib
            -DPNG_PNG_INCLUDE_DIR:FILEPATH=${png_install_dir}/include)
   else()
        set(COMMONS_subproject_CMAKE_ARGS ${COMMONS_subproject_CMAKE_ARGS}
            -DPNG_LIBRARY_RELEASE:FILEPATH=${png_install_dir}/lib/libpng16.a
            -DPNG_LIBRARY_DEBUG:FILEPATH=${png_install_dir}/lib/libpng16d.a
            -DPNG_PNG_INCLUDE_DIR:FILEPATH=${png_install_dir}/include
        )
   endif()
endif()

# this is circumventing some weird CMake shuffeling of dependencies. these libraries always have to go last
# at least once, but multiple times if necessary.
if(UNIX AND NOT APPLE)
    target_link_libraries(Commons::LibZIP INTERFACE Commons::X11 pthread)
    target_link_libraries(Commons::LibPNG INTERFACE Commons::X11 pthread)
endif()

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    BUILDING      O P E N C V
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
add_library (Commons::OpenCV INTERFACE IMPORTED GLOBAL)

if(WIN32)
    set(OpenCV_STATIC ON)
endif()

if(NOT ${COMMONS_BUILD_OPENCV})
    message(STATUS "Trying to find OpenCV...")
    find_package( OpenCV )
    
    if(NOT OpenCV_FOUND)
        set(COMMONS_BUILD_OPENCV ON CACHE BOOL "OpenCV Override" FORCE)
        message(STATUS "Cannot find OpenCV. Trying to build it.")
    else() 
        include_directories( ${OpenCV_INCLUDE_DIRS} )
        message(STATUS "Found OpenCV Version: ${OpenCV_VERSION} include: ${OpenCV_INCLUDE_DIRS} libs: ${OpenCV_LIBRARIES}")
        set(OpenCV_LIBS ${OpenCV_LIBRARIES})
    
        target_link_libraries (Commons::OpenCV INTERFACE ${OpenCV_LIBS})
        if(UNIX AND NOT APPLE)
            find_package(TIFF)
            message(STATUS "TIFF libraries: ${TIFF_LIBRARIES}")
            target_link_libraries(Commons::OpenCV INTERFACE tiff cairo)
        endif()
        set(OpenCV_LIBS Commons::OpenCV)
    endif()
endif()

if(${COMMONS_BUILD_OPENCV})
    set(trigger_build_dir "${CMAKE_BINARY_DIR}/force_opencv")
    set(BUILD_SHARED_LIBS OFF)
    message(STATUS "Building OpenCV...")
    
    if(UNIX)
        get_filename_component(PKG_CONFIG_PATH ${AVDEVICE_LIBRARY} DIRECTORY)
        
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{PKG_CONFIG_PATH})
        set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH}/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        message(STATUS "PKG_CONFIG_PATH: $ENV{PKG_CONFIG_PATH}")
    endif()
    
    set(OPENCV_PREFIX_PATH "${png_install_dir}" "${zlib_install_dir}" ${CMAKE_PREFIX_PATH} )
    message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
   
   if(COMMONS_CONDA_PACKAGE_INSTALL)
		set(COMMONS_CMAKE_BUILD_TYPE Release)
   else()
		set(COMMONS_CMAKE_BUILD_TYPE $<$<CONFIG:Debug>:Release>$<$<CONFIG:Release>:Debug>)
   endif()
   
   message(STATUS "COMMONS_CMAKE_BUILD_TYPE ${COMMONS_CMAKE_BUILD_TYPE}")
   message(STATUS "ALL COMMONS_subproject_CMAKE_ARGS: ${COMMONS_subproject_CMAKE_ARGS}")
   set(OPENCV_CXX_FLAGS "${ORIGINAL_CXX_FLAGS}")
   set(OPENCV_C_FLAGS "${CMAKE_C_FLAGS}")

    if(UNIX AND NOT APPLE)
        set(OPENCV_USE_PCH OFF)
    else()
        set(OPENCV_USE_PCH ON)
    endif()

   if(COMMONS_FOR_JS)
    IF(NOT CMAKE_BUILD_TYPE)
       SET(CMAKE_BUILD_TYPE Release FORCE)
    ENDIF()
    set(OPENCV_CXX_FLAGS "${OPENCV_CXX_FLAGS}")
    set(OPENCV_C_FLAGS "${OPENCV_CXX_FLAGS}")
    set(OPENCV_CMAKE_ARGS 
            -DBUILD_IPP_IW=OFF 
            -DBUILD_ITT=OFF 
            -DWITH_1394=OFF 
            -DWITH_CUDA=OFF 
            -DWITH_JPEG=OFF
            -DWITH_CUFFT=OFF 
            -DWITH_GSTREAMER=OFF 
            -DWITH_IPP=OFF 
            -DWITH_JASPER=OFF 
            -DWITH_WEBP=ON
            -DWITH_OPENEXR=OFF 
            -DWITH_PVAPI=OFF 
            -DWITH_GIGEAPI=OFF 
            -DWITH_QT=ON 
            -DWITH_TIFF=OFF 
            -DWITH_V4L=OFF 
            -DWITH_LIBV4L=OFF 
            -DWITH_OPENCL=OFF 
            -DWITH_OPENCLAMDFFT=OFF 
            -DWITH_OPENCLAMDBLAS=OFF 
            -DWITH_MATLAB=OFF 
            -DWITH_GPHOTO2=OFF 
            -DWITH_LAPACK=OFF 
            -DBUILD_opencv_apps=OFF 
            -DBUILD_ANDROID_EXAMPLES=OFF 
            -DBUILD_DOCS=OFF 
            -DBUILD_PACKAGE=OFF 
            -DBUILD_PERF_TESTS=OFF 
            -DBUILD_TESTS=OFF 
            -DBUILD_FAT_JAVA_LIB=OFF 
            -DENABLE_PRECOMPILED_HEADERS=${OPENCV_USE_PCH}
            -DBUILD_opencv_highgui=ON
            -DBUILD_opencv_dnn=OFF 
            -DBUILD_opencv_video=ON
            -DBUILD_opencv_videoio=ON
            -DBUILD_opencv_flann:BOOL=ON
            -DBUILD_opencv_photo=ON
            -DBUILD_opencv_stitching=OFF 
            -DBUILD_opencv_superres=OFF 
            -DBUILD_opencv_ts=OFF 
            -DBUILD_opencv_videostab=OFF 
            -DBUILD_opencv_aruco=OFF 
            -DBUILD_opencv_bgsegm=OFF 
            -DBUILD_opencv_bioinspired=OFF 
            -DBUILD_opencv_ccalib=OFF 
            -DBUILD_opencv_cnn_3dobj=OFF 
            -DBUILD_opencv_contrib_world=OFF 
            -DBUILD_opencv_cvv=OFF 
            -DBUILD_opencv_datasets=OFF 
            -DBUILD_opencv_dnns_easily_fooled=OFF 
            -DBUILD_opencv_dnn_modern=OFF 
            -DBUILD_opencv_dpm=OFF 
            -DBUILD_opencv_face=OFF 
            -DBUILD_opencv_freetype=OFF 
            -DBUILD_opencv_fuzzy=OFF 
            -DBUILD_opencv_hdf=OFF 
            -DBUILD_opencv_img_hash=OFF 
            -DBUILD_opencv_shape=OFF 
            -DBUILD_opencv_line_descriptor=OFF 
            -DBUILD_opencv_matlab=OFF 
            -DBUILD_opencv_optflow=OFF 
            -DBUILD_opencv_phase_unwrapping=OFF 
            -DBUILD_opencv_plot=OFF 
            -DBUILD_opencv_python_bindings_generator=OFF 
            -DBUILD_opencv_reg=OFF 
            -DBUILD_opencv_rgbd=OFF 
            -DBUILD_opencv_saliency=OFF 
            -DBUILD_opencv_structured_light=OFF 
            -DBUILD_opencv_surface_matching=OFF 
            -DBUILD_opencv_text=OFF 
            -DBUILD_opencv_tracking=OFF 
            -DBUILD_opencv_ximgproc=OFF 
            -DBUILD_opencv_xobjdetect=OFF 
            -DBUILD_opencv_xphoto=OFF
            -DBUILD_JPEG=OFF
            -DWITH_FFMPEG:BOOL=OFF
            -DBUILD_DOCS=OFF
            -DBUILD_EXAMPLES=OFF
            -DBUILD_FAT_JAVA_LIB=OFF
            -DBUILD_IPP_IW=OFF
            -DBUILD_PACKAGE=OFF
            -DBUILD_PERF_TESTS=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTS=OFF
            -DBUILD_OPENJPEG:BOOL=OFF
            -DWITH_OPENJPEG:BOOL=OFF
            -DBUILD_WITH_DEBUG_INFO=OFF
            -DCV_ENABLE_INTRINSICS=OFF
            -DCPU_BASELINE=
            -DCPU_BASELINE_DISABLE=
            -DCPU_DISPATCH=
            -DCV_DISABLE_OPTIMIZATION=ON 
            -DCPU_BASELINE= 
            -DCPU_DISPATCH=
            -DCMAKE_BUILD_TYPE=Release
            -DWITH_PTHREADS_PF=OFF
            -DCMAKE_CONFIGURATION_TYPES:STRING=Release
    )

else()
    set(OPENCV_CXX_FLAGS "${OPENCV_CXX_FLAGS} -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")
    set(OPENCV_C_FLAGS "${OPENCV_C_FLAGS} -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")

    set(OPENCV_CMAKE_ARGS
            -DBUILD_DOCS:BOOL=FALSE
            -DBUILD_EXAMPLES:BOOL=FALSE
            -DBUILD_TESTS:BOOL=FALSE
            -DBUILD_SHARED_LIBS:BOOL=FALSE
            -DBUILD_NEW_PYTHON_SUPPORT:BOOL=OFF
            -DBUILD_WITH_DEBUG_INFO=OFF
            -DWITH_EIGEN:BOOL=FALSE
            -DWITH_CUDA:BOOL=FALSE
            -DWITH_FFMPEG:BOOL=TRUE
            -DWITH_MSMF:BOOL=FALSE
            -DWITH_IPP:BOOL=FALSE
            -DWITH_GTK:BOOL=FALSE
            -DWITH_CAIRO:BOOL=FALSE
            -DWITH_PNG:BOOL=TRUE
            -DWITH_GSTREAMER:BOOL=OFF
            -DWITH_CAROTENE:BOOL=FALSE
            -DBUILD_PERF_TESTS:BOOL=FALSE
            -DBUILD_opencv_apps:BOOL=FALSE
            -DBUILD_opencv_java:BOOL=FALSE
            -DBUILD_opencv_python:BOOL=OFF
            -DBUILD_opencv_objdetect:BOOL=ON
            -DBUILD_opencv_dnn:BOOL=ON
            -DBUILD_opencv_ml:BOOL=OFF
            -DBUILD_opencv_stitching:BOOL=OFF
            -DBUILD_opencv_flann:BOOL=ON
            -DWITH_WEBP=OFF
            -DBUILD_PNG:BOOL=OFF
            -DBUILD_ZLIB:BOOL=OFF
            -DBUILD_PROTOBUF:BOOL=ON
            -DBUILD_JPEG:BOOL=ON
            -DBUILD_OPENJPEG:BOOL=OFF
            -DWITH_OPENJPEG:BOOL=OFF
            -DBUILD_JASPER:BOOL=OFF
            -DBUILD_opencv_python3=OFF
            -DBUILD_opencv_python2=OFF
            -DWITH_JASPER:BOOL=OFF
            -DBUILD_opencv_world:BOOL=OFF
            -DBUILD_opencv_gapi=ON
            -DBUILD_WITH_STATIC_CRT:BOOL=ON
            -DBUILD_FAT_JAVA_LIB=OFF
            -DBUILD_TIFF=ON
            -DWITH_OPENCL=ON
            -DWITH_OPENEXR=OFF
            -DENABLE_PRECOMPILED_HEADERS=${OPENCV_USE_PCH}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_CONFIGURATION_TYPES:STRING=Release)
endif()
    set(OPENCV_CMAKE_ARGS ${OPENCV_CMAKE_ARGS} 
                ${COMMONS_subproject_CMAKE_ARGS}
                -DCMAKE_CXX_FLAGS:STRING=${OPENCV_CXX_FLAGS}
                -DCMAKE_C_FLAGS:STRING=${OPENCV_C_FLAGS}
                -DCMAKE_BUILD_TYPE:STRING=${COMMONS_CMAKE_BUILD_TYPE}
                -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
                "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_SOURCE_DIR}/cmake;${CMAKE_PREFIX_PATH}"
                "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/opencv_install"
                -DCMAKE_DEBUG_POSTFIX=
                ${OPENCV_EXTRA_FLAGS})

    set(COMMONS_OPENCV_VERSION "4.6.0")
    if(APPLE)
        message(STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
        if(NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
            message(STATUS "ARM?")
        else()
            message(STATUS "x86_64 macos")
            set(COMMONS_OPENCV_VERSION "4.6.0")
        endif()
    endif()
    STRING(REPLACE "." "" COMMONS_OPENCV_VERSION_NO_DOTS ${COMMONS_OPENCV_VERSION})
    message(STATUS "Commons using opencv version ${COMMONS_OPENCV_VERSION_NO_DOTS}")

    set(OCV_CMAKE_BUILD_TYPE Release) #COMMONS_CMAKE_BUILD_TYPE

    if(UNIX)
        message(STATUS "Using custom configure command CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${PKG_CONFIG_PATH}/pkgconfig LD_LIBRARY_PATH=$CONDA_PREFIX/lib PKG_CONFIG_LIBDIR=$CONDA_PREFIX/lib ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${OCV_CMAKE_BUILD_TYPE} -G ${CMAKE_GENERATOR} ${OPENCV_CMAKE_ARGS} ${CMAKE_BINARY_DIR}/opencv/src/CustomOpenCV")
        ExternalProject_add(CustomOpenCV
            PREFIX ${CMAKE_BINARY_DIR}/opencv
            GIT_REPOSITORY  https://github.com/opencv/opencv.git
            GIT_TAG ${COMMONS_OPENCV_VERSION}
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${PKG_CONFIG_PATH}/pkgconfig LD_LIBRARY_PATH=$CONDA_PREFIX/lib PKG_CONFIG_LIBDIR=$CONDA_PREFIX/lib ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${OCV_CMAKE_BUILD_TYPE} -G ${CMAKE_GENERATOR} ${OPENCV_CMAKE_ARGS} ${CMAKE_BINARY_DIR}/opencv/src/CustomOpenCV
            BUILD_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${PKG_CONFIG_PATH}/pkgconfig LD_LIBRARY_PATH=$CONDA_PREFIX/lib PKG_CONFIG_LIBDIR=$CONDA_PREFIX/lib ${CMAKE_COMMAND} --build . --config ${OCV_CMAKE_BUILD_TYPE}
            INSTALL_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${PKG_CONFIG_PATH}/pkgconfig LD_LIBRARY_PATH=$CONDA_PREFIX/lib PKG_CONFIG_LIBDIR=$CONDA_PREFIX/lib ${CMAKE_COMMAND} --build . --config ${OCV_CMAKE_BUILD_TYPE} --target install
            CMAKE_ARGS
            ${OPENCV_CMAKE_ARGS})
    else()
        ExternalProject_add(CustomOpenCV
            PREFIX ${CMAKE_BINARY_DIR}/opencv
            GIT_REPOSITORY  https://github.com/opencv/opencv.git
            GIT_TAG ${COMMONS_OPENCV_VERSION}
            UPDATE_COMMAND ""
            CMAKE_ARGS
            ${OPENCV_CMAKE_ARGS})
    endif()

    if(APPLE)
        set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/opencv_install/lib/cmake/opencv4/ ${CMAKE_PREFIX_PATH})
        set(OpenCV_DIR ${CMAKE_BINARY_DIR}/opencv_install/)
    elseif(UNIX)
        set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/opencv_install/share/OpenCV/ ${CMAKE_PREFIX_PATH})
        set(OpenCV_DIR ${CMAKE_BINARY_DIR}/opencv_install/)
    else()
        set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/opencv_install/ ${CMAKE_PREFIX_PATH})
        set(OpenCV_DIR ${CMAKE_BINARY_DIR}/opencv_install/)
    endif()
	
	message(STATUS "OPENCV_EXTRA_FLAGS = ${OPENCV_EXTRA_FLAGS}")
    message(STATUS "OPENCVDIR = ${OpenCV_DIR}")
    
    set(OpenCV_LIB_PATH "")
    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(OpenCV_LIBS opencv_core opencv_imgproc opencv_imgcodecs opencv_flann opencv_photo opencv_video opencv_videoio opencv_highgui opencv_features2d opencv_calib3d)
        set(OpenCV_LIBS ${OpenCV_LIBS} opencv_core opencv_gapi libwebp ade ${FFMPEG_LIBRARIES})
    elseif(WIN32)
        string(REPLACE " " ";" VERSION_LIST ${CMAKE_GENERATOR})
        list(GET VERSION_LIST 2 VSTD_VERSION_MAJOR)
        
        math (EXPR MSVC_TOOLSET_MAJOR "${MSVC_VERSION}/10")
        message(STATUS "CMAKE_GENERATOR_TOOLSET ${MSVC_TOOLSET_MAJOR} ${CMAKE_GENERATOR} ${VSTD_VERSION_MAJOR}")
        message("Including OpenCV lib path: ${CMAKE_BINARY_DIR}/opencv_install/x64/vc${VSTD_VERSION_MAJOR}/staticlib")
        set(OpenCV_LIB_PATH ${CMAKE_BINARY_DIR}/opencv_install/x64/vc${VSTD_VERSION_MAJOR}/staticlib)
        link_directories(${OpenCV_LIB_PATH})
        set(OpenCV_LIBS opencv_core${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_imgproc${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_imgcodecs${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_video${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_videoio${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_highgui${COMMONS_OPENCV_VERSION_NO_DOTS} 
            opencv_features2d${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_flann${COMMONS_OPENCV_VERSION_NO_DOTS} opencv_calib3d${COMMONS_OPENCV_VERSION_NO_DOTS} 
            libjpeg-turbo ittnotify ade libprotobuf libtiff quirc ${FFMPEG_LIBRARIES})
    else()
        set(OpenCV_LIBS opencv_core opencv_imgproc opencv_imgcodecs opencv_flann opencv_photo opencv_video opencv_videoio opencv_highgui opencv_features2d opencv_calib3d)
		set(OpenCV_LIBS ${OpenCV_LIBS} opencv_core libjpeg-turbo ittnotify ade libprotobuf quirc ${FFMPEG_LIBRARIES})
		if(APPLE)
			set(OpenCV_LIBS ${OpenCV_LIBS} libtiff)
		else()
			set(OpenCV_LIBS ${OpenCV_LIBS} libtiff z) #raw1394 usb-1.0 udev glib-2.0)
	#		set(OpenCV_LIBS ${OpenCV_LIBS} opencv_stitching opencv_photo)
		endif()
    endif()

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/opencv_install/include)
    target_include_directories(Commons::OpenCV INTERFACE
      "$<BUILD_INTERFACE:${png_install_dir}/include>"
      "$<INSTALL_INTERFACE:${CMAKE_BINARY_DIR}/opencv_install/lib;${OpenCV_LIB_PATH}>"
    )
    
    message(STATUS "Find OpenCV ${OpenCV_DIR} ${COMMONS_OPENCV_VERSION}")
	find_package(OpenCV ${COMMONS_OPENCV_VERSION} PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)
	if(OpenCV_FOUND)
		set(OpenCV_LIBS ${OpenCV_LIBRARIES})
		message(STATUS "Found opencv at ${OpenCV_INSTALL_PATH}, using its config.cmake")
	endif()
    
    if(UNIX)
        target_include_directories(Commons::OpenCV INTERFACE ${CMAKE_BINARY_DIR}/opencv_install/include/opencv4 )
    else()
        target_include_directories(Commons::OpenCV INTERFACE ${CMAKE_BINARY_DIR}/opencv_install/include )
    endif()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/opencv_install/include/opencv4)
        target_include_directories(Commons::OpenCV INTERFACE
            ${CMAKE_BINARY_DIR}/opencv_install/include/opencv4
        )
    endif()

    message(STATUS "Adding path: ${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty")
    target_link_directories(Commons::OpenCV INTERFACE 
        ${CMAKE_BINARY_DIR}/opencv_install/lib 
        ${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty)
    
    target_link_libraries (Commons::OpenCV INTERFACE ${OpenCV_LIBS} Commons::LibPNG Commons::LibZIP Commons::LibZ)
    add_dependencies(Commons::OpenCV CustomOpenCV Commons::LibPNG Commons::LibZIP Commons::LibZ)
    message(STATUS "OpenCV: ${OpenCV_LIBS}")
    set(OpenCV_LIBS Commons::OpenCV)

    if(APPLE)
        # make directories to avoid warning messages in Xcode
        message(STATUS "Making directories at ${CMAKE_BINARY_DIR}/opencv_install/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/RelWithDebInfo")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/Release")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/Debug")

        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty/RelWithDebInfo")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty/Release")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty/Debug")
    endif()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set_target_properties(Commons::OpenCV PROPERTIES
          IMPORTED_LOCATION "${OpenCV_LIBS}"
          INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/opencv_install/include;${CMAKE_BINARY_DIR}/opencv_install/include/opencv4"
          INTERFACE_LINK_DIRECTORIES "${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty;${CMAKE_BINARY_DIR}/opencv_install/lib;${OpenCV_LIB_PATH}"
        )
    else()
        set_target_properties(Commons::OpenCV PROPERTIES
          IMPORTED_LOCATION "${OpenCV_LIBS}"
          INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/opencv_install/include"
          INTERFACE_LINK_DIRECTORIES "${CMAKE_BINARY_DIR}/opencv_install/lib/opencv4/3rdparty;${CMAKE_BINARY_DIR}/opencv_install/lib;${OpenCV_LIB_PATH}"
        )
    endif()
endif()

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()


message(STATUS "hidden visibility in commons project ${CMAKE_CXX_FLAGS}")

add_library(Commons::HTTPD INTERFACE IMPORTED GLOBAL)
if(COMMONS_BUILD_HTTPD)
	set(httpd_PREFIX "${CMAKE_BINARY_DIR}/lmhttpd")
	set(httpd_INSTALL_DIR "${CMAKE_BINARY_DIR}/lmhttpd")
	set(httpd_CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${httpd_INSTALL_DIR})
    message(STATUS "Adding microhttpd")
	ExternalProject_Add(LibMicroHttpd
	  PREFIX ${httpd_PREFIX}
	  GIT_REPOSITORY https://git.gnunet.org/libmicrohttpd.git
	  GIT_TAG v0.9.75
	  #SOURCE_DIR ${httpd_PREFIX}/
	  BINARY_DIR ${httpd_PREFIX}/src/LibMicroHttpd/
	  INSTALL_DIR ${httpd_INSTALL_DIR}
	  CONFIGURE_COMMAND cd ${httpd_PREFIX}/src/LibMicroHttpd/ && autoreconf -fi && ./configure --bindir=${httpd_INSTALL_DIR}/lib --prefix=${httpd_INSTALL_DIR}
	  UPDATE_COMMAND ""
	)
    add_custom_command(TARGET LibMicroHttpd POST_BUILD
        COMMAND mkdir -p "${httpd_INSTALL_DIR}/lib/Release")
    add_custom_command(TARGET LibMicroHttpd POST_BUILD
        COMMAND mkdir -p "${httpd_INSTALL_DIR}/lib/Debug")

    target_link_libraries(Commons::HTTPD INTERFACE microhttpd)

	message(STATUS "HTTPD dir ${httpd_INSTALL_DIR}/lib")
    file(MAKE_DIRECTORY "${httpd_INSTALL_DIR}/include")
    
    target_compile_definitions(Commons::HTTPD INTERFACE WITH_MHD=true)
    target_include_directories(Commons::HTTPD INTERFACE
      "$<BUILD_INTERFACE:${httpd_INSTALL_DIR}/include>"
      "$<INSTALL_INTERFACE:${httpd_INSTALL_DIR}/lib>"
    )
    set_target_properties(Commons::HTTPD PROPERTIES
      IMPORTED_LOCATION "microhttpd"
      TARGET_INCLUDE_DIRECTORIES "${httpd_INSTALL_DIR}/include"
      TARGET_LINK_DIRECTORIES "${httpd_INSTALL_DIR}/lib"
    )
    add_dependencies(Commons::HTTPD LibMicroHttpd)

else()
    target_compile_definitions(Commons::HTTPD INTERFACE WITH_MHD=false)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cnpy)

if(APPLE)
    # make directories to avoid warning messages in Xcode
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cnpy")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cnpy/RelWithDebInfo")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cnpy/Release")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cnpy/Debug")
endif()


add_subdirectory(common)
add_subdirectory(examples)

if(POLICY CMP0114)
  cmake_policy(POP)
endif()

message(STATUS "ZLIB: ${COMMONS_BUILD_ZLIB} ${COMMONS_BUILD_ZIP} ${LIBZIP_LIBRARY}")

INSTALL(TARGETS "commons"
EXPORT "commonsTargets" 
ARCHIVE DESTINATION lib COMPONENT libs
RUNTIME DESTINATION bin COMPONENT libs
LIBRARY DESTINATION lib COMPONENT libs
PUBLIC_HEADER DESTINATION include/${PROJECT_NAME} COMPONENT devel
INCLUDES DESTINATION include
)


if(WIN32)
    if(COMMONS_BUILD_OPENCV)
        install(
            FILES
            "${OpenCV_LIB_PATH}/../bin/opencv_videoio_ffmpeg${COMMONS_OPENCV_VERSION_NO_DOTS}_64.dll" DESTINATION bin)
    endif()
endif()

export(TARGETS commons cnpy imgui ${pybind11_LIBRARY} NAMESPACE Commons FILE commons.cmake EXPORT_LINK_INTERFACE_LIBRARIES )
