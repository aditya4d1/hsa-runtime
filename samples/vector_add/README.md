##Compiling OpenCL kernels to AMD LLVM IR

<code>
clang-3.6 -Xclang -mlink-bitcode-file -Xclang /usr/lib/clc/kaveri-r600--.bc -Dcl_clang_storage_class_specifiers -target amdgcn--amdhsa -mcpu=kaveri -S -emit-llvm -o vadd.ll -x cl vadd.cl
</code>
