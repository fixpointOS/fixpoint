#include "addblob_fixpoint.h"

typedef struct Z_env_module_instance_t {
  Z_addblob_module_instance_t* module_instance;
  wasm_rt_memory_t ro_mem_0;
  wasm_rt_memory_t ro_mem_1;
  wasm_rt_memory_t rw_mem_0;
} Z_env_module_instance_t;

wasm_rt_memory_t* Z_env_Z_ro_mem_0(Z_env_module_instance_t* env_module_instance) {
  return &env_module_instance->ro_mem_0;
}

wasm_rt_memory_t* Z_env_Z_ro_mem_1(Z_env_module_instance_t* env_module_instance) {
  return &env_module_instance->ro_mem_1;
}

wasm_rt_memory_t* Z_env_Z_rw_mem_0(Z_env_module_instance_t* env_module_instance) {
 return &env_module_instance->rw_mem_0;
}

void init_rw_mems(Z_env_module_instance_t* env_module_instance) {
  wasm_rt_allocate_memory(&env_module_instance->rw_mem_0, 1, 65536);
  return;
}

extern void fixpoint_get_tree_entry(void*, uint32_t, uint32_t, uint32_t);
void Z_addblob_Z_env_Z_get_tree_entry(struct Z_env_module_instance_t* env_module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle) {
  fixpoint_get_tree_entry(env_module_instance->module_instance, src_ro_handle, entry_num, target_ro_handle);
}

extern void fixpoint_attach_blob(void*, uint32_t, wasm_rt_memory_t*);
void attach_blob_0(Z_env_module_instance_t* env_module_instance, uint32_t ro_handle) {
  wasm_rt_memory_t* ro_mem = &env_module_instance->ro_mem_0;
  fixpoint_attach_blob(env_module_instance->module_instance, ro_handle, ro_mem);
}

void attach_blob_1(Z_env_module_instance_t* env_module_instance, uint32_t ro_handle) {
  wasm_rt_memory_t* ro_mem = &env_module_instance->ro_mem_1;
  fixpoint_attach_blob(env_module_instance->module_instance, ro_handle, ro_mem);
}

void Z_addblob_Z_env_Z_attach_blob(struct Z_env_module_instance_t* env_module_instance, uint32_t ro_handle, uint32_t ro_mem_num) {
  if (ro_mem_num == 0) {
    attach_blob_0(env_module_instance, ro_handle);
    return;
  }
  if (ro_mem_num == 1) {
    attach_blob_1(env_module_instance, ro_handle);
    return;
  }
  wasm_rt_trap(WASM_RT_TRAP_OOB);
}

extern void fixpoint_detach_mem( void*, wasm_rt_memory_t*, uint32_t );
void detach_mem_0(Z_env_module_instance_t* env_module_instance, uint32_t rw_handle) {
  wasm_rt_memory_t* rw_mem = &env_module_instance->rw_mem_0;
  fixpoint_detach_mem(env_module_instance->module_instance, rw_mem, rw_handle);
}

void Z_addblob_Z_env_Z_detach_mem(struct Z_env_module_instance_t* env_module_instance, uint32_t rw_mem_num, uint32_t rw_handle) {
  if (rw_mem_num == 0) {
    detach_mem_0(env_module_instance, rw_handle);
    return;
  }
  wasm_rt_trap(WASM_RT_TRAP_OOB);
}

extern void fixpoint_freeze_blob( void*, uint32_t, uint32_t, uint32_t );
void Z_addblob_Z_env_Z_freeze_blob(struct Z_env_module_instance_t* env_module_instance, uint32_t rw_handle, uint32_t size, uint32_t ro_handle) {
  fixpoint_freeze_blob(env_module_instance->module_instance, rw_handle, size, ro_handle);
}

extern void fixpoint_designate_output( void*, uint32_t );
void Z_addblob_Z_env_Z_designate_output(struct Z_env_module_instance_t* env_module_instance, uint32_t ro_handle) {
  fixpoint_designate_output(env_module_instance->module_instance, ro_handle);
}

extern void* fixpoint_init_module_instance(size_t, void* encode_name);
extern void* fixpoint_init_env_module_instance(size_t);
void* initProgram(void* encode_name) {
  Z_addblob_module_instance_t* instance = (Z_addblob_module_instance_t*)fixpoint_init_module_instance(sizeof(Z_addblob_module_instance_t), encode_name);
  Z_env_module_instance_t* env_module_instance = fixpoint_init_env_module_instance(sizeof(Z_env_module_instance_t));
  init_rw_mems(env_module_instance);
  env_module_instance->module_instance = instance;
  Z_addblob_init_module();
  Z_addblob_init(instance, env_module_instance);
  return (void*)instance;
}

void* executeProgram(void* instance) {
  Z_addblob_Z__fixpoint_apply((Z_addblob_module_instance_t*)instance);
  return instance;
}
