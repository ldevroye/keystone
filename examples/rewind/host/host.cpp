//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"
#include <vector>
#include <cstring>

using namespace Keystone;

enum {
  OCALL_PRINT_BUFFER = 1,
  OCALL_LOAD_COUNTER = 8,
  OCALL_SAVE_COUNTER = 9,
};

static std::vector<uint8_t> saved_blob;

void host_print(const char* str)
{
  printf("[HOST] %s\n", str);
}

static void save_counter_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_size == 0) {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  saved_blob.resize(arg_size);
  memcpy(saved_blob.data(), (void*)arg_ptr, arg_size);
  char to_prt[80];
  sprintf(to_prt, "saved sealed blob size = %zu", arg_size);
  host_print(to_prt);
  
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

static void load_counter_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  if (saved_blob.empty()) {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  /* Copy opaque saved blob into the shared return buffer
   * The enclave cannot dereference host heap pointers */
  void* return_buffer = (void*)edge_call_data_ptr();
  memcpy(return_buffer, saved_blob.data(), saved_blob.size());
  edge_call_setup_ret(edge_call, return_buffer, saved_blob.size());
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

static void print_buffer_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_ptr == 0 || arg_size == 0) {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  printf("%.*s", (int)arg_size, (char*)arg_ptr);
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

  static Keystone::Error configure_enclave(Enclave& enclave, Params& params, char** argv)
  {
    Keystone::Error init_ret = enclave.init(argv[1], argv[2], argv[3], params);
    if (init_ret != Keystone::Error::Success) {
      host_print("Error loading the enclave");
      return init_ret;
    }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  register_call(OCALL_PRINT_BUFFER, print_buffer_dispatch);
  register_call(OCALL_SAVE_COUNTER, save_counter_dispatch);
  register_call(OCALL_LOAD_COUNTER, load_counter_dispatch);
  edge_call_init_internals(
    (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  return Keystone::Error::Success;
  }

int
main(int argc, char** argv) 
{
  Params params;
  params.setFreeMemSize(256 * 1024);
  params.setUntrustedSize(256 * 1024);

  uintptr_t ret;
  const auto success = Keystone::Error::Success;

  auto counter = 0;
  const auto max_run = 10;
  while ((ret != 0 || counter==0) && // counter = 0 => initial enclave => result is null
         counter < max_run) 
  {
    counter++;
    Enclave enclave;

    host_print("configuring enclave");
    if (configure_enclave(enclave, params, argv) != success) {
      return 1;
    }

    host_print("starting enclave");
    enclave.run(&ret);

    if (ret != 0) {
      host_print("enclave returned non-success, retrying");
    }
  }

  if (counter==max_run){
    host_print("too many runs, exiting");
  } else {
    host_print("run completed");
  }
  
  return ret;
}
