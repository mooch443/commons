# FindZip
# -------
#
# Finds libzip and defines:
#
#   Zip_FOUND
#   ZIP_VERSION
#   ZIP_INCLUDE_DIRS
#   ZIP_LIBRARIES
#   ZIP::ZIP

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_ZIP QUIET libzip)
endif()

find_path(ZIP_INCLUDE_DIR
    NAMES zip.h
    HINTS ${PC_ZIP_INCLUDE_DIRS})
find_library(ZIP_LIBRARY
    NAMES zip libzip
    HINTS ${PC_ZIP_LIBRARY_DIRS})

set(ZIP_VERSION "${PC_ZIP_VERSION}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zip
    REQUIRED_VARS ZIP_LIBRARY ZIP_INCLUDE_DIR
    VERSION_VAR ZIP_VERSION)

if(Zip_FOUND)
    set(ZIP_FOUND TRUE)
    set(ZIP_INCLUDE_DIRS "${ZIP_INCLUDE_DIR}")
    set(ZIP_LIBRARIES "${ZIP_LIBRARY}")

    if(NOT TARGET ZIP::ZIP)
        add_library(ZIP::ZIP UNKNOWN IMPORTED)
        set_target_properties(ZIP::ZIP PROPERTIES
            IMPORTED_LOCATION "${ZIP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZIP_INCLUDE_DIR}")
        if(PC_ZIP_CFLAGS_OTHER)
            set_property(TARGET ZIP::ZIP PROPERTY
                INTERFACE_COMPILE_OPTIONS "${PC_ZIP_CFLAGS_OTHER}")
        endif()
    endif()
endif()

mark_as_advanced(ZIP_INCLUDE_DIR ZIP_LIBRARY)
