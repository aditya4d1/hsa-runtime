  
__kernel void hello_world(__global float *out) {
	*out = 3.14159f;	

#if 0
	/* Inline assembly example of this kernel */
	__asm__ volatile("s_load_dwordx2 s[0:1], s[0:1] 0x0" : : : "s0", "s1");
	__asm__ volatile("v_mov_b32 v0, 3.14159" : : : "v0");
	__asm__ volatile("s_waitcnt lgkmcnt(0)");
	__asm__ volatile("v_mov_b32 v1, s0" : : : "v1");
	__asm__ volatile("v_mov_b32 v2, s1" : : : "v2");
	__asm__ volatile("flat_store_dword v0, v[1:2]" : : : "v0");
	__asm__ volatile("s_endpgm");
#endif
}
