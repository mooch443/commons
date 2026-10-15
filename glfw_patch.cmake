foreach(required_variable IN ITEMS GLFW_SOURCE_DIR GIT_EXECUTABLE GLFW_PATCH_FILE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be set for the GLFW patch step.")
    endif()
endforeach()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${GLFW_PATCH_FILE}"
    WORKING_DIRECTORY "${GLFW_SOURCE_DIR}"
    RESULT_VARIABLE glfw_patch_check_result
    ERROR_VARIABLE glfw_patch_check_error
)

if(glfw_patch_check_result EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${GLFW_PATCH_FILE}"
        WORKING_DIRECTORY "${GLFW_SOURCE_DIR}"
        RESULT_VARIABLE glfw_patch_result
        ERROR_VARIABLE glfw_patch_error
    )
    if(NOT glfw_patch_result EQUAL 0)
        message(FATAL_ERROR "Could not apply the GLFW monitor recovery patch:\n${glfw_patch_error}")
    endif()
    message(STATUS "Applied GLFW monitor recovery patch")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${GLFW_PATCH_FILE}"
    WORKING_DIRECTORY "${GLFW_SOURCE_DIR}"
    RESULT_VARIABLE glfw_reverse_check_result
    ERROR_VARIABLE glfw_reverse_check_error
)

if(glfw_reverse_check_result EQUAL 0)
    message(STATUS "GLFW monitor recovery patch already applied")
    return()
endif()

message(FATAL_ERROR
    "The GLFW monitor recovery patch is neither applicable nor already applied.\n"
    "Apply check:\n${glfw_patch_check_error}\n"
    "Reverse check:\n${glfw_reverse_check_error}"
)
