//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"

using namespace Keystone;

void host_print(char* str)
{
  printf("[HOST] %s\n", str);
}

int
main(int argc, char** argv) {
    Enclave enclave;
    Params params;

    params.setFreeMemSize(256 * 1024);
    params.setUntrustedSize(256 * 1024);

    enclave.init(argv[1], argv[2], argv[3], params);

    enclave.registerOcallDispatch(incoming_call_dispatch);
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
    Keystone::Error ret = enclave.run();

    if (ret != Keystone::Error::Success) {
        host_print("error returned");
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
