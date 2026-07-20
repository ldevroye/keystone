set(eapp_bin rewind)
set(eapp_src
  eapp/rewind.c
  eapp/checkpoint.c
  eapp/crypto.c
  eapp/fault.c
  ${KEYSTONE_EYRIE_RUNTIME}/crypto/aes.c) # already implemented crypto scheme
set(host_bin rewind-runner)
set(host_src host/host.cpp)
set(package_name "rewind.ke")
set(package_script "./rewind-runner rewind eyrie-rt loader.bin")
set(eyrie_plugins "io_syscall linux_syscall env_setup")



###################### macros definitions (can be left empty) ######################
set(FAULT_PERIOD "50" CACHE STRING "optional override for PERIOD macro (example: 30ULL)")
set(FAULT_RANDOMIZE_SEED "1" CACHE STRING "optional override for FAULT_RANDOMIZE_SEED macro (0 or 1)")
set(FAULT_SEED "" CACHE STRING "optional override for SEED macro (example: 0x6b656973746f6e68ULL)")
set(EAPP_RUNS "100" CACHE STRING "optional override for the eapp main loop bound (example: 50)")
set(HOST_MAX_RUN "20" CACHE STRING "optional override for host main loop bound before stopping execution (example: 10)")
set(HOST_LOGGING "1" CACHE STRING "optional override for host logging (0 or 1)")
set(EAPP_LOGGING "1" CACHE STRING "optional override for enclave logging (0 or 1)")
set(ENABLE_TESTING "1" CACHE STRING "enable analysis mode with summary-only logs (0 or 1)")
set(ANALYSIS_RUNS "10" CACHE STRING "number of executions to average over in analysis mode")



###################### EAPP ######################
add_executable(${eapp_bin} ${eapp_src})
target_link_libraries(${eapp_bin} "-nostdlib -static" ${KEYSTONE_LIB_EAPP} ${KEYSTONE_LIB_EDGE})

# PRIVATE is for end use of the option, meaning only these will have the tag
# compiler option that keeps stack frames easier to inspect for
target_compile_options(${eapp_bin} PRIVATE -fno-omit-frame-pointer)

# build-time switch that enables page-crypto code path in the generated Eyrie runtime
target_compile_options(${eapp_bin} PRIVATE -DUSE_PAGE_CRYPTO)

if(NOT "${FAULT_PERIOD}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE PERIOD=${FAULT_PERIOD})
endif()

if(NOT "${FAULT_RANDOMIZE_SEED}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE FAULT_RANDOMIZE_SEED=${FAULT_RANDOMIZE_SEED})
endif()

if(NOT "${FAULT_SEED}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE SEED=${FAULT_SEED})
endif()

if(NOT "${EAPP_RUNS}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE EAPP_RUNS=${EAPP_RUNS})
endif()

if(NOT "${EAPP_LOGGING}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE EAPP_LOGGING=${EAPP_LOGGING})
endif()

if(NOT "${ENABLE_TESTING}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE ENABLE_TESTING=${ENABLE_TESTING})
endif()

if(NOT "${ANALYSIS_RUNS}" STREQUAL "")
  target_compile_definitions(${eapp_bin} PRIVATE ANALYSIS_RUNS=${ANALYSIS_RUNS})
endif()

# PUBLIC is for project dependency level, meaning the whole project will have these included
target_include_directories(${eapp_bin}
  PUBLIC ${KEYSTONE_SDK_DIR}/include/app
  PUBLIC ${KEYSTONE_SDK_DIR}/include/edge
  PUBLIC ${KEYSTONE_EYRIE_RUNTIME}/include)




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
add_eyrie_runtime(${eapp_bin}-eyrie
  ${eyrie_plugins}
  ${eyrie_files_to_copy})

# add target for packaging (see keystone.cmake)

add_keystone_package(${eapp_bin}-package
  ${package_name}
  ${package_script}
  ${eyrie_files_to_copy} ${eapp_bin} ${host_bin})

add_dependencies(${eapp_bin}-package ${eapp_bin}-eyrie)

# add package to the top-level target
add_dependencies(examples ${eapp_bin}-package)
