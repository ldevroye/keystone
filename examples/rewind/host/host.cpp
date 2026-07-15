//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"
#include <vector>
#include <cstring>

using namespace Keystone;

// number of try before stopping the rewind
#define MAX_RUNS 10

enum 
{
  OCALL_PRINT_BUFFER = 1,
  OCALL_LOAD_CHECKPOINT_BLOB = 8,
  OCALL_SAVE_CHECKPOINT_BLOB = 9,
};

static std::vector<uint8_t> saved_checkpoint_blob;

void host_print(const char* str)
{
  printf("[HOST] %s\n", str);
}

static void save_checkpoint_blob_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  // copy the sealed blob out of the enclave shared buffer into host memory
  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_size == 0) 
  {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  saved_checkpoint_blob.resize(arg_size);
  memcpy(saved_checkpoint_blob.data(), (void*)arg_ptr, arg_size);
  char to_prt[80];
  sprintf(to_prt, "saved checkpoint blob size = %zu", arg_size);
  host_print(to_prt);
  
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

static void load_checkpoint_blob_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  
  if (saved_checkpoint_blob.empty()) 
  {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  // copy the opaque blob back through the shared return buffer, not a host pointer
  void* return_buffer = (void*)edge_call_data_ptr();
  memcpy(return_buffer, saved_checkpoint_blob.data(), saved_checkpoint_blob.size());
  edge_call_setup_ret(edge_call, return_buffer, saved_checkpoint_blob.size());
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

static void print_buffer_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_ptr == 0 || arg_size == 0) 
  {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  printf("%.*s", (int)arg_size, (char*)arg_ptr);
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

// each fresh enclave instance must register the same ocalls before it runs
static Keystone::Error configure_enclave(Enclave& enclave, Params& params, char** argv)
{
  Keystone::Error init_ret = enclave.init(argv[1], argv[2], argv[3], params);
  
  if (init_ret != Keystone::Error::Success) 
  {
    host_print("Error loading the enclave");
    return init_ret;
  }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  register_call(OCALL_PRINT_BUFFER, print_buffer_dispatch);
  register_call(OCALL_SAVE_CHECKPOINT_BLOB, save_checkpoint_blob_dispatch);
  register_call(OCALL_LOAD_CHECKPOINT_BLOB, load_checkpoint_blob_dispatch);
  edge_call_init_internals(
    (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  return Keystone::Error::Success;
}

int main(int argc, char** argv) 
{
  Params params;
  params.setFreeMemSize(256 * 1024);
  params.setUntrustedSize(256 * 1024);

  uintptr_t ret;
  const auto success = Keystone::Error::Success;

  auto counter = 0;
  while ((ret != 0 || counter==0) && // counter = 0 => initial enclave => result is null
         counter <= MAX_RUNS) 
  {
    counter++;
    Enclave enclave;

    host_print("configuring enclave");
    if (configure_enclave(enclave, params, argv) != success) 
    {
      return 1;
    }

    host_print("starting enclave");
    enclave.run(&ret);

    if (ret != 0) 
    {
      host_print("enclave returned non-success, retrying");
    }
  }

  if (counter==MAX_RUNS+1)
  {
    host_print("too many runs, exiting");
  } else {
    host_print("run completed");
  }
  
  return ret;
}
