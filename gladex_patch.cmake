if(NOT DEFINED GLADEX_SOURCE_DIR OR GLADEX_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "GLADEX_SOURCE_DIR must be set for the gladex patch step.")
endif()

set(GLADEX_FILES_INIT "${GLADEX_SOURCE_DIR}/glad/files/__init__.py")

if(NOT EXISTS "${GLADEX_FILES_INIT}")
    message(FATAL_ERROR "gladex patch target not found: ${GLADEX_FILES_INIT}")
endif()

file(READ "${GLADEX_FILES_INIT}" GLADEX_FILES_CONTENTS)

set(GLADEX_OPEN_LOCAL_OLD "    return open(local_path, *args, **kwargs)")
set(GLADEX_OPEN_LOCAL_NEW "    if not args and \"mode\" not in kwargs:\n        kwargs[\"mode\"] = \"rb\"\n    return open(local_path, *args, **kwargs)")

string(FIND "${GLADEX_FILES_CONTENTS}" "${GLADEX_OPEN_LOCAL_NEW}" GLADEX_PATCH_ALREADY_APPLIED)
if(NOT GLADEX_PATCH_ALREADY_APPLIED EQUAL -1)
    message(STATUS "gladex packaged XML loader already patched")
    return()
endif()

string(FIND "${GLADEX_FILES_CONTENTS}" "${GLADEX_OPEN_LOCAL_OLD}" GLADEX_PATCH_TARGET_INDEX)
if(GLADEX_PATCH_TARGET_INDEX EQUAL -1)
    message(FATAL_ERROR "Could not find gladex open_local() fallback to patch.")
endif()

string(REPLACE "${GLADEX_OPEN_LOCAL_OLD}" "${GLADEX_OPEN_LOCAL_NEW}" GLADEX_FILES_CONTENTS "${GLADEX_FILES_CONTENTS}")
file(WRITE "${GLADEX_FILES_INIT}" "${GLADEX_FILES_CONTENTS}")

message(STATUS "Patched gladex packaged XML loader to default to binary mode")
