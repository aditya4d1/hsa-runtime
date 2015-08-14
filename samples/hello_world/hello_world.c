
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hsa.h>

#define FILENAME "hello"

hsa_status_t find_gpu_device(hsa_agent_t agent, void *data);

static void print_usage() {

  fprintf(stderr, "Usage: hello_world <binary_file>\n");

}

int main (int argc, char **argv) {

  hsa_status_t status = hsa_init();
  int err;
  long input_size;
  size_t read_size;
  FILE *input;
  char *input_data;
  hsa_code_object_t code_object;
  hsa_agent_t device;
  char device_name[64];
  uint32_t queue_size = 0;
  hsa_queue_t *queue;
  hsa_executable_t executable;
  hsa_executable_symbol_t kernel_symbol;
  uint64_t code_handle;
  hsa_signal_t signal;
  hsa_kernel_dispatch_packet_t aql;
  float *out;
  const char *filename;

  assert(status == HSA_STATUS_SUCCESS);

  if (argc != 2) {
    print_usage();
    return 1;
  }  

  filename = argv[1];

  input = fopen(filename, "r");
  assert(input);

  err = fseek(input, 0L, SEEK_END);
  assert(err == 0);

  input_size = ftell(input);
  assert(input_size != -1);

  err = fseek(input, 0L, SEEK_SET);
  assert(err == 0);

  input_data = malloc(input_size);
  assert(input_data);

  read_size = fread(input_data, 1, input_size, input);
  assert(read_size == input_size);

  err = fclose(input);
  assert(err == 0);

  // Deserialize:
  status = hsa_code_object_deserialize(input_data, input_size, NULL, &code_object);  
  assert(status == HSA_STATUS_SUCCESS);
  assert(code_object.handle != 0);
  free(input_data);

  // Find GPU
  status = hsa_iterate_agents(find_gpu_device, &device);
  assert(status == HSA_STATUS_SUCCESS);

  // Print Device name
  status = hsa_agent_get_info(device, HSA_AGENT_INFO_NAME, device_name);
  assert(status == HSA_STATUS_SUCCESS);
  printf("Using <%s>\n", device_name);

  // Get queue size
  status == hsa_agent_get_info(device, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
  assert(status == HSA_STATUS_SUCCESS);

  // Create command queue
  status = hsa_queue_create(device, queue_size, HSA_QUEUE_TYPE_MULTI, NULL, NULL,
                            0, 0, &queue);
  assert(status == HSA_STATUS_SUCCESS);

  // Create executable
  status = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN,
                                 NULL, &executable);
  assert(status == HSA_STATUS_SUCCESS); 

  // Load code object
  status = hsa_executable_load_code_object(executable, device, code_object, NULL);
  assert(status == HSA_STATUS_SUCCESS);

  // Freeze executable
  status = hsa_executable_freeze(executable, NULL); 
  assert(status == HSA_STATUS_SUCCESS);

  // Get symbol handle
  status = hsa_executable_get_symbol(executable, NULL, "hello_world", device,
                                     0, &kernel_symbol);
  assert(status == HSA_STATUS_SUCCESS);

  // Get code handle
  status = hsa_executable_symbol_get_info(kernel_symbol,
                                          HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT,
                                          &code_handle);
  assert(status == HSA_STATUS_SUCCESS);

  // Ge a signal.
  status = hsa_signal_create(1, 0, NULL, &signal);
  assert(status == HSA_STATUS_SUCCESS);

  // Setup dispatch packet
  memset(&aql, 0, sizeof(aql));

  // Setup dispatch size and fences.
  aql.completion_signal = signal;
  aql.setup = 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
  aql.workgroup_size_x = 1;
  aql.workgroup_size_y = 1;
  aql.workgroup_size_z = 1;
  aql.grid_size_x = 1;
  aql.grid_size_y = 1;
  aql.grid_size_z = 1; 
  aql.header =
      (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
      (1 << HSA_PACKET_HEADER_BARRIER) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

  aql.group_segment_size = 0;
  aql.private_segment_size = 0;

  // Setup kernel arguments;
  out = calloc(64, sizeof(float));
  status = hsa_memory_register(out, 64 * 4);
  assert(status == HSA_STATUS_SUCCESS);

  // Build kernel arguments and kernel code.
  aql.kernel_object = code_handle;
  aql.kernarg_address = &out; 

  // Register argument buffer
  status = hsa_memory_register(&out, sizeof(&out));
  assert(status == HSA_STATUS_SUCCESS);

  // Write to command queue
  {
    const uint32_t queue_mask = queue->size - 1;
    uint64_t index = hsa_queue_load_write_index_relaxed(queue);
    ((hsa_kernel_dispatch_packet_t*)(queue->base_address))[ index & queue_mask] = aql;
    hsa_queue_store_write_index_relaxed(queue, index + 1);

    // Ring door bell
    hsa_signal_store_relaxed(queue->doorbell_signal, index);

    if (hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_LT, 1, ~0ULL,
                                HSA_WAIT_STATE_ACTIVE) != 0) {
      fprintf(stderr, "Signal wait returned unexpected value\n");
      exit(1);
    }

    hsa_signal_store_relaxed(signal, 1);
  }

  printf("pi = %f\n", out[0]);
  return 0;
}

hsa_status_t find_gpu_device(hsa_agent_t agent, void *data) {
  if (data == NULL) {
     return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_device_type_t hsa_device_type;
  hsa_status_t hsa_error_code = hsa_agent_get_info(
    agent, HSA_AGENT_INFO_DEVICE, &hsa_device_type
  );
  if (hsa_error_code != HSA_STATUS_SUCCESS) {
    return hsa_error_code;
  }

  if (hsa_device_type == HSA_DEVICE_TYPE_GPU) {
    *((hsa_agent_t*)data) = agent;
  }

  return HSA_STATUS_SUCCESS;
}
