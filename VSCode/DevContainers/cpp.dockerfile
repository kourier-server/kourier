# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

FROM mcr.microsoft.com/devcontainers/cpp:debian-13

ENV LANG=C.UTF-8
ARG THREAD_COUNT
ARG LLVM_VERSION

ENV LLVM_INSTALLATION_DIR=/opt/llvm/${LLVM_VERSION}

RUN --mount=target=/var/lib/apt/lists,type=cache \
    --mount=target=/var/cache/apt,type=cache \
    --mount=target=/builds,type=cache \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/); \
    export LLVM_MAJOR_VERSION=$(echo "$LLVM_VERSION" | awk -F '.' '{print $1}'); \
    DEBIAN_FRONTEND=noninteractive apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y upgrade; \
    DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential lsb-release cmake git gpg wget ninja-build libssl-dev protobuf-compiler libprotobuf-dev python3 meson llvm clang lld libclang-rt-dev libc++abi-dev libc++-dev doxygen; \
    export LLVM_HOST_TARGET=$(llvm-config --host-target); \
    mkdir -p /opt/llvm; \
    mkdir -p /builds/llvm; \
    cd /builds/llvm; \
    git clone --depth 1 -c advice.detachedHead=false --branch llvmorg-${LLVM_VERSION} https://github.com/llvm/llvm-project; \
    cd llvm-project; \
    mkdir build; \
    cd build; \
    export CFLAGS="-Wno-unused-command-line-argument -fPIC -fno-omit-frame-pointer"; \
    export CXXFLAGS="-Wno-unused-command-line-argument -fPIC -fno-omit-frame-pointer -stdlib=libc++ -std=c++20"; \
    export LDFLAGS="-Wno-unused-command-line-argument -fno-omit-frame-pointer -rtlib=compiler-rt -stdlib=libc++ -Wl,-rpath,${LLVM_INSTALLATION_DIR}/lib -Wl,-rpath,${LLVM_INSTALLATION_DIR}/lib/${LLVM_HOST_TARGET} -Wl,-rpath,${LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}/lib/${LLVM_HOST_TARGET}"; \
    cmake -GNinja ../llvm -DCMAKE_INSTALL_PREFIX="${LLVM_INSTALLATION_DIR}" -DCMAKE_BUILD_TYPE="Release" -DLLVM_PARALLEL_LINK_JOBS="1" -DCMAKE_C_COMPILER="clang" -DCMAKE_CXX_COMPILER="clang++" -DLLVM_USE_LINKER="lld" -DLLVM_ENABLE_LIBCXX="ON" -DLLVM_ENABLE_PIC="ON" -DLLVM_BUILD_LLVM_DYLIB="ON" -DLLVM_LINK_LLVM_DYLIB="ON" -DLLVM_INSTALL_TOOLCHAIN_ONLY="ON" -DLLVM_ENABLE_LTO="OFF" -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_DEFAULT_TARGET_TRIPLE="${LLVM_HOST_TARGET}" -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld;lldb" -DLLVM_ENABLE_RUNTIMES="compiler-rt;libunwind;libcxxabi;libcxx" -DCOMPILER_RT_BUILD_BUILTINS="ON" -DCOMPILER_RT_USE_ATOMIC_LIBRARY="ON" -DCOMPILER_RT_USE_LLVM_UNWINDER="ON" -DCOMPILER_RT_USE_BUILTINS_LIBRARY="ON" -DCOMPILER_RT_HAS_GCC_S_LIB="OFF" -DCOMPILER_RT_BUILD_SANITIZERS="ON" -DCOMPILER_RT_BUILD_SHARED_ASAN="ON" -DCOMPILER_RT_BUILD_SHARED_TSAN="ON" -DCOMPILER_RT_BUILD_SHARED_MSAN="ON" -DLIBUNWIND_USE_COMPILER_RT="ON" -DLIBCXX_USE_COMPILER_RT="ON" -DLIBCXX_HAS_ATOMIC_LIB="OFF" -DLIBCXX_HAS_GCC_LIB="OFF" -DLIBCXX_HAS_GCC_S_LIB="OFF" -DLIBCXXABI_USE_COMPILER_RT="ON" -DLIBCXXABI_USE_LLVM_UNWINDER="ON" -DCLANG_DEFAULT_RTLIB="compiler-rt" -DCLANG_DEFAULT_UNWINDLIB="libunwind" -DCLANG_DEFAULT_CXX_STDLIB="libc++" -DCLANG_DEFAULT_LINKER="lld" -DCLANG_DEFAULT_PIE_ON_LINUX="ON" -DLLVM_INCLUDE_EXAMPLES="OFF" -DLLVM_INCLUDE_TESTS="OFF" -DLLVM_INSTALL_BINUTILS_SYMLINKS="ON"; \
    cmake --build . --parallel ${THREAD_COUNT} -- compiler-rt unwind clang lld lldb cxxabi cxx; \
    cmake --build . --target install --parallel ${THREAD_COUNT}; \
    apt-get clean; \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/)

ENV LLVM_VERSION=${LLVM_VERSION}

ENTRYPOINT ["/usr/bin/bash"]
