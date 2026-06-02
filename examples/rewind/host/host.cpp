//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"

using namespace Keystone;

enum {
  OCALL_LOAD_COUNTER = 8,
  OCALL_SAVE_COUNTER = 9,
};

static int saved_counter = 0;

void host_print(char* str)
{
  printf("[HOST] %s\n", str);
}

static void save_counter_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_size < sizeof(saved_counter)) {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  saved_counter = *(int*)arg_ptr;
  char to_prt[50];
  sprintf(to_prt, "counter = %d", saved_counter);
  host_print(to_prt);
  
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

static void load_counter_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  int* return_counter = (int*)edge_call_data_ptr();

  *return_counter = saved_counter;
  edge_call_setup_ret(edge_call, return_counter, sizeof(saved_counter));
  edge_call->return_data.call_status = CALL_STATUS_OK;
}

int
main(int argc, char** argv) {
    Enclave enclave;
    Params params;

    params.setFreeMemSize(256 * 1024);
    params.setUntrustedSize(256 * 1024);

    enclave.init(argv[1], argv[2], argv[3], params);

    enclave.registerOcallDispatch(incoming_call_dispatch);
    register_call(OCALL_SAVE_COUNTER, save_counter_dispatch);
    register_call(OCALL_LOAD_COUNTER, load_counter_dispatch);
    edge_call_init_internals(
        (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

    /*
    Keystone::Error ret = enclave.run();

    switch (ret)
    {
      case Keystone::Error::Success:
        host_print("well finished");
        break;
      case Keystone::Error::EnclaveInterrupted:
        host_print("interupted");
        break;
      default:
        Enclave backup;
        host_print("need backup");
        backup.init(argv[1], argv[2], argv[3], params);

        backup.registerOcallDispatch(incoming_call_dispatch);
        edge_call_init_internals((uintptr_t)backup.getSharedBuffer(), backup.getSharedBufferSize());

        backup.run();

        ret = backup.run();
        break;
    }
    */
    host_print("runnin main");
    

    Keystone::Error ret = enclave.run();


    if (ret != Keystone::Error::Success) {
        host_print("returned succesfully");
    } else {
        host_print("enclave exited, restarting");
    }

    Enclave backup;

    backup.init(argv[1], argv[2], argv[3], params);

    backup.registerOcallDispatch(incoming_call_dispatch);

    edge_call_init_internals(
        (uintptr_t)backup.getSharedBuffer(),
        backup.getSharedBufferSize());

    ret = backup.run();

  return static_cast<int>(ret);
}
