//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"
#include "host.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdint>



void save_checkpoint_blob_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t arg_ptr;
  size_t arg_size;

  auto start = chrono::steady_clock::now();

  // copy the sealed blob out of the enclave shared buffer into host memory
  if (edge_call_args_ptr(edge_call, &arg_ptr, &arg_size) != 0 ||
      arg_size == 0) 
  {
    edge_call->return_data.call_status = CALL_STATUS_ERROR;
    return;
  }

  saved_checkpoint_blob.resize(arg_size);
  memcpy(saved_checkpoint_blob.data(), (void*)arg_ptr, arg_size);
#if !ENABLE_TESTING && HOST_LOGGING
  char to_prt[80];
  sprintf(to_prt, "saved checkpoint blob size = %zu", arg_size);
  host_print_if_not_testing(to_prt);
#endif

  auto end = chrono::steady_clock::now();
  const auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
#if ENABLE_TESTING
  update_timing_stats(save_stats, static_cast<uint64_t>(elapsed_us));
#endif
  
  edge_call->return_data.call_status = CALL_STATUS_OK;
}



void load_checkpoint_blob_dispatch(void* buffer)
{
  struct edge_call* edge_call = (struct edge_call*)buffer;

  auto start = chrono::steady_clock::now();
  
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

  auto end = chrono::steady_clock::now();
  const auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
#if ENABLE_TESTING
  update_timing_stats(load_stats, static_cast<uint64_t>(elapsed_us));
#endif
}



void print_buffer_dispatch(void* buffer)
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
Error configure_enclave(Enclave& enclave, Params& params, char** argv)
{
  Error init_ret = enclave.init(argv[1], argv[2], argv[3], params);
  
  if (init_ret != Error::Success) 
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

  return Error::Success;
}



int main(int argc, char** argv) 
{
  Params params;
  params.setFreeMemSize(256 * 1024);
  params.setUntrustedSize(256 * 1024);

  print_test_parameters();

  uintptr_t ret = 0;
  const auto success = Error::Success;

  const auto host_retry_limit = MAX_RUNS;
  const auto analysis_iteration_limit = ENABLE_TESTING ? ANALYSIS_RUNS : 1;
  auto analysis_counter = 0;
  while (analysis_counter < analysis_iteration_limit)
  {
    analysis_counter++;
    saved_checkpoint_blob.clear(); // clear checkpoint
    run_stats = {};
    auto retry_counter = 0;

    while ((ret != 0 || retry_counter == 0) && // retry_counter = 0 => initial enclave => result is null
           retry_counter < host_retry_limit)
    {
      retry_counter++;
      Enclave enclave;

      auto run_start = chrono::steady_clock::now();
      host_print_if_not_testing("configuring enclave");
      if (configure_enclave(enclave, params, argv) != success) 
      {
        return 1;
      }

      host_print_if_not_testing("starting enclave");
      enclave.run(&ret);
      auto run_end = chrono::steady_clock::now();
      auto run_ms = chrono::duration_cast<chrono::milliseconds>(run_end - run_start).count();
#if ENABLE_TESTING
      update_timing_stats(run_stats, static_cast<uint64_t>(run_ms));
#else
      char run_log[128];
      snprintf(run_log, sizeof(run_log), "analysis_run=%d retry=%d return_val=%lu duration_ms=%lld",
                   analysis_counter, retry_counter, (unsigned long)ret, (long long)run_ms);
      host_print(run_log);
#endif

      if (ret != 0) 
      {
        host_print_if_not_testing("enclave returned non-success, retrying");
      }
    }

#if ENABLE_TESTING
    print_analysis_run_summary(analysis_counter);
#endif
  }

#if ENABLE_TESTING
  print_timing_stats("save_checkpoint", save_stats);
  print_timing_stats("load_checkpoint", load_stats);
#endif

  if (ret != 0)
  {
    host_print_if_not_testing("too many runs, exiting");
  } else {
    host_print_if_not_testing("run completed");
  }
  
  return ret;
}
