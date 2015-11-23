#define __global __attribute__((address_space(1)))

__kernel void vecadd(__global int *A,
                     __global int *B,
                     __global int *C) {
int idx = get_global_id(0);
C[idx] = A[idx] + B[idx];
}
