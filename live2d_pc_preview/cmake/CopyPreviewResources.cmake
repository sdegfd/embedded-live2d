function(copy_required_file source_path destination_path)
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "Required preview resource missing: ${source_path}")
  endif()

  get_filename_component(destination_directory "${destination_path}" DIRECTORY)
  file(MAKE_DIRECTORY "${destination_directory}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source_path}" "${destination_path}")
endfunction()

function(copy_required_directory source_path destination_path)
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "Required preview resource directory missing: ${source_path}")
  endif()

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_path}" "${destination_path}")
endfunction()

if(NOT DEFINED TARGET_DIR)
  message(FATAL_ERROR "TARGET_DIR is required.")
endif()

if(NOT DEFINED WORKSPACE_ROOT)
  message(FATAL_ERROR "WORKSPACE_ROOT is required.")
endif()

set(WYZ_SOURCE_MODEL_STEM "wyz_202605071436_42667 拷贝")
set(WYZ_TARGET_STEM "wyz_preview")
set(WYZ_TARGET_DIR "${TARGET_DIR}/${WYZ_TARGET_STEM}")

file(MAKE_DIRECTORY "${WYZ_TARGET_DIR}")
copy_required_file("${WORKSPACE_ROOT}/${WYZ_SOURCE_MODEL_STEM}.model3.json" "${WYZ_TARGET_DIR}/${WYZ_TARGET_STEM}.model3.json")
copy_required_file("${WORKSPACE_ROOT}/${WYZ_SOURCE_MODEL_STEM}.moc3" "${WYZ_TARGET_DIR}/${WYZ_SOURCE_MODEL_STEM}.moc3")
copy_required_file("${WORKSPACE_ROOT}/${WYZ_SOURCE_MODEL_STEM}.cdi3.json" "${WYZ_TARGET_DIR}/${WYZ_SOURCE_MODEL_STEM}.cdi3.json")
copy_required_directory("${WORKSPACE_ROOT}/${WYZ_SOURCE_MODEL_STEM}.512" "${WYZ_TARGET_DIR}/${WYZ_SOURCE_MODEL_STEM}.512")

set(MARK_SOURCE_DIR "${WORKSPACE_ROOT}/mark_free_zh/runtime")
set(MARK_TARGET_STEM "mark_preview")
set(MARK_TARGET_DIR "${TARGET_DIR}/${MARK_TARGET_STEM}")

file(MAKE_DIRECTORY "${MARK_TARGET_DIR}")
copy_required_file("${MARK_SOURCE_DIR}/mark_free_t04.model3.json" "${MARK_TARGET_DIR}/${MARK_TARGET_STEM}.model3.json")
copy_required_file("${MARK_SOURCE_DIR}/mark_free_t04.moc3" "${MARK_TARGET_DIR}/mark_free_t04.moc3")
copy_required_file("${MARK_SOURCE_DIR}/mark_free_t04.cdi3.json" "${MARK_TARGET_DIR}/mark_free_t04.cdi3.json")
copy_required_file("${MARK_SOURCE_DIR}/mark_free_t04.physics3.json" "${MARK_TARGET_DIR}/mark_free_t04.physics3.json")
copy_required_directory("${MARK_SOURCE_DIR}/mark_free_t04.2048" "${MARK_TARGET_DIR}/mark_free_t04.2048")
copy_required_directory("${MARK_SOURCE_DIR}/motion" "${MARK_TARGET_DIR}/motion")