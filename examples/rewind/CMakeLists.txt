set(eapp_bin rewind)
set(eapp_src
  eapp/rewind.c
  eapp/checkpoint.c
  eapp/crypto.c
  ${KEYSTONE_EYRIE_RUNTIME}/crypto/aes.c) # already implemented crypto scheme
set(host_bin rewind-runner)
set(host_src host/host.cpp)
set(package_name "rewind.ke")
set(package_script "./rewind-runner rewind eyrie-rt loader.bin")
set(eyrie_plugins "io_syscall linux_syscall env_setup")

# eapp

add_executable(${eapp_bin} ${eapp_src})
target_link_libraries(${eapp_bin} "-nostdlib -static" ${KEYSTONE_LIB_EAPP} ${KEYSTONE_LIB_EDGE})

# PRIVATE is for end use of the option, meaning only these will have the tag
# compiler option that keeps stack frames easier to inspect for
target_compile_options(${eapp_bin} PRIVATE -fno-omit-frame-pointer)

# build-time switch that enables page-crypto code path in the generated Eyrie runtime
target_compile_options(${eapp_bin} PRIVATE -DUSE_PAGE_CRYPTO)

# PUBLIC is for project dependency level, meaning the whole project will have these included
target_include_directories(${eapp_bin}
  PUBLIC ${KEYSTONE_SDK_DIR}/include/app
  PUBLIC ${KEYSTONE_SDK_DIR}/include/edge
  PUBLIC ${KEYSTONE_EYRIE_RUNTIME}/include)

# host

add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})
set_target_properties(${host_bin}
  PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(${host_bin}
  PUBLIC ${KEYSTONE_SDK_DIR}/include/common
  PUBLIC ${KEYSTONE_SDK_DIR}/include/host
  PUBLIC ${KEYSTONE_SDK_DIR}/include/edge)

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
