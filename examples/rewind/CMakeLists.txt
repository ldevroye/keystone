set(main_eapp_bin rewind)
set(fallback_eapp_bin rewind-fallback)
set(eapp_src
  eapp/rewind.c
  eapp/checkpoint.c
  eapp/crypto.c
  eapp/fault.c
  ${KEYSTONE_EYRIE_RUNTIME}/crypto/aes.c) # already implemented crypto scheme
set(host_bin rewind-runner)
set(host_src host/host.cpp)
set(package_name "rewind.ke")
set(package_script "./rewind-runner ${main_eapp_bin} eyrie-rt loader.bin ${fallback_eapp_bin}")
set(eyrie_plugins "io_syscall linux_syscall env_setup")



###################### macros definitions (can be left empty) ######################
set(FAULT_PERIOD "30" CACHE STRING "optional override for PERIOD macro (example: 30ULL)")
set(FAULT_RANDOMIZE_SEED "1" CACHE STRING "optional override for FAULT_RANDOMIZE_SEED macro (0 or 1)")
set(FAULT_SEED "" CACHE STRING "optional override for SEED macro (example: 0x6b656973746f6e68ULL)")
set(EAPP_RUNS "50" CACHE STRING "optional override for the eapp main loop bound (example: 50)")
set(HOST_MAX_RUN "20" CACHE STRING "optional override for host main loop bound before stopping execution (example: 10)")
set(HOST_LOGGING "1" CACHE STRING "optional override for host logging (0 or 1)")
set(EAPP_LOGGING "1" CACHE STRING "optional override for enclave logging (0 or 1)")
set(ENABLE_TESTING "0" CACHE STRING "enable analysis mode with summary-only logs (0 or 1)")
set(ANALYSIS_RUNS "10" CACHE STRING "number of executions to average over in analysis mode")



###################### EAPP ######################
function(configure_rewind_eapp target_name enclave_label)
  add_executable(${target_name} ${eapp_src})
  target_link_libraries(${target_name} "-nostdlib -static" ${KEYSTONE_LIB_EAPP} ${KEYSTONE_LIB_EDGE})

  # private keeps the frame layout stable for inspection during fault tracing
  target_compile_options(${target_name} PRIVATE -fno-omit-frame-pointer)

  # page crypto stays enabled for both identities
  target_compile_options(${target_name} PRIVATE -DUSE_PAGE_CRYPTO)
  target_compile_definitions(${target_name} PRIVATE ENCLAVE_LABEL="${enclave_label}")

  if(NOT "${FAULT_PERIOD}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE PERIOD=${FAULT_PERIOD})
  endif()

  if(NOT "${FAULT_RANDOMIZE_SEED}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE FAULT_RANDOMIZE_SEED=${FAULT_RANDOMIZE_SEED})
  endif()

  if(NOT "${FAULT_SEED}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE SEED=${FAULT_SEED})
  endif()

  if(NOT "${EAPP_RUNS}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE EAPP_RUNS=${EAPP_RUNS})
  endif()

  if(NOT "${EAPP_LOGGING}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE EAPP_LOGGING=${EAPP_LOGGING})
  endif()

  if(NOT "${ENABLE_TESTING}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE ENABLE_TESTING=${ENABLE_TESTING})
  endif()

  if(NOT "${ANALYSIS_RUNS}" STREQUAL "")
    target_compile_definitions(${target_name} PRIVATE ANALYSIS_RUNS=${ANALYSIS_RUNS})
  endif()

  # public includes are shared by both enclave identities
  target_include_directories(${target_name}
    PUBLIC ${KEYSTONE_SDK_DIR}/include/app
    PUBLIC ${KEYSTONE_SDK_DIR}/include/edge
    PUBLIC ${KEYSTONE_EYRIE_RUNTIME}/include)
endfunction()

configure_rewind_eapp(${main_eapp_bin} MAIN)
configure_rewind_eapp(${fallback_eapp_bin} FALLBACK)




###################### HOST ######################
add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

if(NOT "${HOST_MAX_RUN}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE MAX_RUNS=${HOST_MAX_RUN})
endif()

if(NOT "${HOST_LOGGING}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE HOST_LOGGING=${HOST_LOGGING})
  target_compile_definitions(${host_bin} PRIVATE EAPP_LOGGING=${EAPP_LOGGING})
endif()

if(NOT "${FAULT_PERIOD}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE PERIOD=${FAULT_PERIOD})
endif()

if(NOT "${ENABLE_TESTING}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE ENABLE_TESTING=${ENABLE_TESTING})
endif()

if(NOT "${ANALYSIS_RUNS}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE ANALYSIS_RUNS=${ANALYSIS_RUNS})
endif()

if(NOT "${FAULT_RANDOMIZE_SEED}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE FAULT_RANDOMIZE_SEED=${FAULT_RANDOMIZE_SEED})
endif()

if(NOT "${FAULT_SEED}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE SEED=${FAULT_SEED})
endif()

if(NOT "${EAPP_RUNS}" STREQUAL "")
  target_compile_definitions(${host_bin} PRIVATE EAPP_RUNS=${EAPP_RUNS})
endif()

set_target_properties(${host_bin}
  PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(${host_bin}
  PUBLIC ${KEYSTONE_SDK_DIR}/include/common
  PUBLIC ${KEYSTONE_SDK_DIR}/include/host
  PUBLIC ${KEYSTONE_SDK_DIR}/include/edge)



###################### RUNTIME ######################
# add target for Eyrie runtime (see keystone.cmake)
set(eyrie_files_to_copy .options_log eyrie-rt loader.bin)
add_eyrie_runtime(${main_eapp_bin}-eyrie
  ${eyrie_plugins}
  ${eyrie_files_to_copy})

# add target for packaging (see keystone.cmake)

add_keystone_package(${main_eapp_bin}-package
  ${package_name}
  ${package_script}
  ${eyrie_files_to_copy} ${main_eapp_bin} ${fallback_eapp_bin} ${host_bin})

add_dependencies(${main_eapp_bin}-package ${main_eapp_bin}-eyrie)

# add package to the top-level target
add_dependencies(examples ${main_eapp_bin}-package)
