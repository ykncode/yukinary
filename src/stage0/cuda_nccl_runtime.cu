// ============================================================
// Yukinary NCCL Runtime (Multi-GPU & Distributed AI)
// ============================================================
// NVIDIA Collective Communication Library (NCCL) hooks for compiler.

#include <cuda_runtime.h>
#include <nccl.h>
#include <stdio.h>
#include <stdlib.h>

#define NCCLCHECK(cmd) do {                         \
  ncclResult_t res = cmd;                           \
  if (res != ncclSuccess) {                         \
    printf("NCCL Failure: %s\n", ncclGetErrorString(res)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

#define CUDACHECK(cmd) do {                         \
  cudaError_t e = cmd;                              \
  if (e != cudaSuccess) {                           \
    printf("CUDA Failure: %s\n", cudaGetErrorString(e)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

extern "C" {

// Global Communicator state
static ncclComm_t* comms = NULL;
static int nDev = 0;

void yukinary_nccl_init(int num_gpus) {
    nDev = num_gpus;
    comms = (ncclComm_t*)malloc(sizeof(ncclComm_t) * nDev);
    int* devs = (int*)malloc(sizeof(int) * nDev);
    for (int i = 0; i < nDev; ++i) {
        devs[i] = i;
    }
    
    NCCLCHECK(ncclCommInitAll(comms, nDev, devs));
    free(devs);
}

void yukinary_nccl_allreduce(float** send_buffs, float** recv_buffs, int size) {
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaSetDevice(i));
        NCCLCHECK(ncclAllReduce((const void*)send_buffs[i], (void*)recv_buffs[i], size, ncclFloat, ncclSum, comms[i], 0)); // 0 stream
    }
    NCCLCHECK(ncclGroupEnd());
    
    // Synchronize
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaSetDevice(i));
        CUDACHECK(cudaDeviceSynchronize());
    }
}

void yukinary_nccl_bcast(float** sendrecv_buffs, int size, int root) {
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaSetDevice(i));
        NCCLCHECK(ncclBcast((void*)sendrecv_buffs[i], size, ncclFloat, root, comms[i], 0));
    }
    NCCLCHECK(ncclGroupEnd());
    
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaSetDevice(i));
        CUDACHECK(cudaDeviceSynchronize());
    }
}

void yukinary_nccl_destroy() {
    if (comms) {
        for (int i = 0; i < nDev; ++i) {
            ncclCommDestroy(comms[i]);
        }
        free(comms);
        comms = NULL;
    }
}

} // extern "C"
