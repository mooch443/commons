foreach(required_variable IN ITEMS IMGUI_SOURCE_DIR GIT_EXECUTABLE IMGUI_PATCH_FILE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be set for the ImGui patch step.")
    endif()
endforeach()

if(NOT EXISTS "${IMGUI_SOURCE_DIR}")
    message(FATAL_ERROR "ImGui source directory not found: ${IMGUI_SOURCE_DIR}")
endif()

if(NOT EXISTS "${IMGUI_PATCH_FILE}")
    message(FATAL_ERROR "ImGui patch file not found: ${IMGUI_PATCH_FILE}")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --unidiff-zero "${IMGUI_PATCH_FILE}"
    WORKING_DIRECTORY "${IMGUI_SOURCE_DIR}"
    RESULT_VARIABLE imgui_patch_check_result
    OUTPUT_VARIABLE imgui_patch_check_output
    ERROR_VARIABLE imgui_patch_check_error
)

if(imgui_patch_check_result EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --unidiff-zero "${IMGUI_PATCH_FILE}"
        WORKING_DIRECTORY "${IMGUI_SOURCE_DIR}"
        RESULT_VARIABLE imgui_patch_result
        OUTPUT_VARIABLE imgui_patch_output
        ERROR_VARIABLE imgui_patch_error
    )

    if(NOT imgui_patch_result EQUAL 0)
        message(FATAL_ERROR
            "The ImGui patch passed validation but could not be applied.\n"
            "${imgui_patch_output}${imgui_patch_error}"
        )
    endif()

    message(STATUS "Applied TRex ImGui patch")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --unidiff-zero "${IMGUI_PATCH_FILE}"
    WORKING_DIRECTORY "${IMGUI_SOURCE_DIR}"
    RESULT_VARIABLE imgui_reverse_check_result
    OUTPUT_VARIABLE imgui_reverse_check_output
    ERROR_VARIABLE imgui_reverse_check_error
)

if(imgui_reverse_check_result EQUAL 0)
    message(STATUS "TRex ImGui patch already applied")
    return()
endif()

message(FATAL_ERROR
    "The TRex ImGui patch is neither applicable nor already applied.\n"
    "Apply check:\n${imgui_patch_check_output}${imgui_patch_check_error}\n"
    "Reverse check:\n${imgui_reverse_check_output}${imgui_reverse_check_error}"
)
