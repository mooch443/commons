include_guard(GLOBAL)

function(commons_detect_macos_version out_var)
    set(macos_version "")
    set(macos_version_source "")

    if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND NOT "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
        set(macos_version "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        set(macos_version_source "deployment_target")
    elseif(DEFINED ENV{MACOSX_DEPLOYMENT_TARGET} AND NOT "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
        set(macos_version "$ENV{MACOSX_DEPLOYMENT_TARGET}")
        set(macos_version_source "environment")
    elseif(APPLE)
        if(NOT CMAKE_OSX_SYSROOT)
            message(FATAL_ERROR "No OSX sysroot was set.")
        endif()

        get_filename_component(osx_dir "${CMAKE_OSX_SYSROOT}" NAME)
        string(REPLACE ".sdk" "" osx_dir "${osx_dir}")
        string(REPLACE "MacOSX" "" osx_dir "${osx_dir}")

        if("${osx_dir}" STREQUAL "")
            set(osx_settings "${CMAKE_OSX_SYSROOT}/SDKSettings.plist")
            if(EXISTS "${osx_settings}")
                execute_process(
                    COMMAND /usr/libexec/PlistBuddy -c "Print :Version" "${osx_settings}"
                    OUTPUT_VARIABLE osx_dir
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
            endif()
        endif()

        if("${osx_dir}" STREQUAL "")
            execute_process(
                COMMAND xcrun --sdk macosx --show-sdk-version
                OUTPUT_VARIABLE osx_dir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()

        set(macos_version "${osx_dir}")
        set(macos_version_source "sdk")
    endif()

    if("${macos_version}" STREQUAL "")
        message(WARNING "Could not determine macOS version from sysroot or deployment target (${CMAKE_OSX_DEPLOYMENT_TARGET}).")
    else()
        if(macos_version_source STREQUAL "deployment_target" OR macos_version_source STREQUAL "environment")
            message(STATUS "Found OSX deployment target: '${macos_version}'")
        else()
            message(STATUS "OSX Target: ${macos_version}")
        endif()

        string(REPLACE "." ";" VERSION_LIST "${macos_version}")
        list(LENGTH VERSION_LIST VERSION_LIST_LEN)
        if(VERSION_LIST_LEN GREATER 0)
            list(GET VERSION_LIST 0 COMMONS_MACOS_VERSION_MAJOR)
            set(COMMONS_MACOS_VERSION_MAJOR "${COMMONS_MACOS_VERSION_MAJOR}" PARENT_SCOPE)
        endif()
        if(VERSION_LIST_LEN GREATER 1)
            list(GET VERSION_LIST 1 COMMONS_MACOS_VERSION_MINOR)
        else()
            set(COMMONS_MACOS_VERSION_MINOR 0)
        endif()
        set(COMMONS_MACOS_VERSION_MINOR "${COMMONS_MACOS_VERSION_MINOR}" PARENT_SCOPE)
    endif()

    set(${out_var} "${macos_version}" PARENT_SCOPE)
endfunction()
