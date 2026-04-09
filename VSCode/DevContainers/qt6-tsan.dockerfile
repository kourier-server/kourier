# Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

FROM vscode-dev-container:cpp-tsan

ARG THREAD_COUNT
ARG QT_VERSION
ENV SANITIZED_INSTALL_ROOT_DIR="/opt/root-sanitized"
# We disable halting on error to prevent build/tools from failing.
ENV TSAN_OPTIONS=report_bugs=0:exitcode=0:atexit_sleep_ms=0:halt_on_error=0:report_thread_leaks=0:report_destroy_locked=0:report_signal_unsafe=0:die_after_fork=0:ignore_interceptors_accesses=1:ignore_noninstrumented_modules=1


# Set up environment variables. We use non-instrumented clang with
# resource-dir set to instrumented resources to make large sanitized
# builds like Qt much faster.
RUN --mount=target=/var/lib/apt/lists,type=cache \
    --mount=target=/var/cache/apt,type=cache \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    touch /opt/env-vars.sh; \
    SYS_TARGET_TRIPLET="$(gcc -dumpmachine)"; \
    LLVM_HOST_TARGET="$(llvm-config --host-target)"; \
    LLVM_MAJOR_VERSION="$(echo "$LLVM_VERSION" | awk -F '.' '{print $1}')"; \
    echo "export SYS_TARGET_TRIPLET=\"${SYS_TARGET_TRIPLET}\"" > /opt/env-vars.sh; \
    echo "export LLVM_HOST_TARGET=\"${LLVM_HOST_TARGET}\"" >> /opt/env-vars.sh; \
    echo "export LLVM_MAJOR_VERSION=\"${LLVM_MAJOR_VERSION}\"" >> /opt/env-vars.sh; \
    echo "export PATH=\"${LLVM_INSTALLATION_DIR}/bin:$PATH\"" >> /opt/env-vars.sh; \
    echo "export CC=\"clang\"" >> /opt/env-vars.sh; \
    echo "export CXX=\"clang++\"" >> /opt/env-vars.sh; \
    echo "export LD=\"lld\"" >> /opt/env-vars.sh; \
    echo "export CFLAGS=\"-Wno-unused-command-line-argument -fPIC -fno-omit-frame-pointer -fsanitize=thread -fsanitize-recover=all -resource-dir=\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}\"\"" >> /opt/env-vars.sh; \
    echo "export CXXFLAGS=\"-Wno-unused-command-line-argument -fPIC -fno-omit-frame-pointer -stdlib=libc++ -std=c++20 -fsanitize=thread -fsanitize-recover=all -stdlib++-isystem ${SANITIZED_LLVM_INSTALLATION_DIR}/include/c++/v1 -stdlib++-isystem ${SANITIZED_LLVM_INSTALLATION_DIR}/include/${LLVM_HOST_TARGET}/c++/v1 -resource-dir=\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}\"\"" >> /opt/env-vars.sh; \
    echo "export LDFLAGS=\"-Wno-unused-command-line-argument -fno-omit-frame-pointer -rtlib=compiler-rt -shared-libsan -stdlib=libc++ -fsanitize=thread -fsanitize-recover=all -resource-dir=\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}\" -Wl,-rpath,\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib\" -Wl,-rpath,\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/${LLVM_HOST_TARGET}\" -Wl,-rpath,\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}/lib/${LLVM_HOST_TARGET}\" -L\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib\" -L\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/${LLVM_HOST_TARGET}\" -L\"${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}/lib/${LLVM_HOST_TARGET}\" -Wl,-rpath,\"${SANITIZED_INSTALL_ROOT_DIR}/lib\" -Wl,-rpath,\"${SANITIZED_INSTALL_ROOT_DIR}/lib64\" -Wl,-rpath,\"${SANITIZED_INSTALL_ROOT_DIR}/lib/${SYS_TARGET_TRIPLET}\" -L\"${SANITIZED_INSTALL_ROOT_DIR}/lib\" -L\"${SANITIZED_INSTALL_ROOT_DIR}/lib64\" -L\"${SANITIZED_INSTALL_ROOT_DIR}/lib/${SYS_TARGET_TRIPLET}\"\"" >> /opt/env-vars.sh; \
    LIBRARY_PATH="${SANITIZED_LLVM_INSTALLATION_DIR}/lib:${SANITIZED_LLVM_INSTALLATION_DIR}/lib/${LLVM_HOST_TARGET}:${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}/lib/${LLVM_HOST_TARGET}:${SANITIZED_INSTALL_ROOT_DIR}/lib:${SANITIZED_INSTALL_ROOT_DIR}/lib64:${SANITIZED_INSTALL_ROOT_DIR}/lib/${SYS_TARGET_TRIPLET}:$LD_LIBRARY_PATH"; \
    echo "export LIBRARY_PATH=\"${LIBRARY_PATH}\"" >> /opt/env-vars.sh; \
    echo "export CMAKE_LIBRARY_PATH=\"$LIBRARY_PATH:${CMAKE_LIBRARY_PATH}\"" >> /opt/env-vars.sh; \
    CMAKE_PREFIX_PATH="${SANITIZED_LLVM_INSTALLATION_DIR}:${SANITIZED_LLVM_INSTALLATION_DIR}/lib/clang/${LLVM_MAJOR_VERSION}:${SANITIZED_INSTALL_ROOT_DIR}:${CMAKE_PREFIX_PATH}"; \
    echo "export CMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\"" >> /opt/env-vars.sh; \
    echo "source /opt/env-vars.sh" >> /root/.profile; \
    echo "source /opt/env-vars.sh" >> /root/.bashrc; \
    echo "source /opt/env-vars.sh" >> /home/vscode/.profile; \
    echo "source /opt/env-vars.sh" >> /home/vscode/.bashrc; \
    chmod a=+r /opt/env-vars.sh; \
    touch /opt/source-env-vars.sh; \
    echo "#!/bin/bash" > /opt/source-env-vars.sh; \
    echo "source /opt/env-vars.sh" >> /opt/source-env-vars.sh; \
    echo "exec \"\$@\"" >> /opt/source-env-vars.sh; \
    chmod a=+xr /opt/source-env-vars.sh; \
    DEBIAN_CODE_NAME=$(lsb_release --short --codename); \
    echo "Types: deb-src" > /etc/apt/sources.list.d/debian-src.sources; \
    echo "URIs: http://deb.debian.org/debian" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Suites: ${DEBIAN_CODE_NAME} ${DEBIAN_CODE_NAME}-updates" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Components: main" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Types: deb-src" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "URIs: http://deb.debian.org/debian-security" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Suites: ${DEBIAN_CODE_NAME}-security" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Components: main" >> /etc/apt/sources.list.d/debian-src.sources; \
    echo "Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp" >> /etc/apt/sources.list.d/debian-src.sources; \
    DEBIAN_FRONTEND=noninteractive apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y autoremove; \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/)

SHELL ["/bin/bash", "-l", "-c"]

# Build Qt Core dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache \
    --mount=target=/var/cache/apt,type=cache \
    --mount=target=/builds,type=cache \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/); \
    DEBIAN_FRONTEND=noninteractive apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y upgrade; \
    mkdir -p /builds/debian-src-packages/lz4; \
    cd /builds/debian-src-packages/lz4; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y lz4; \
    mkdir lz4-src; \
    tar -xf lz4_*.orig.tar.gz -C lz4-src; \
    cd lz4-src; \
    LZ4_VERSION=$(ls | awk -F '-' '{print $2}'); \
    cd lz4-${LZ4_VERSION}; \
    make -j${THREAD_COUNT}; \
    make install PREFIX=${SANITIZED_INSTALL_ROOT_DIR}; \
    mkdir -p /builds/debian-src-packages/zlib; \
    cd /builds/debian-src-packages/zlib; \
    git clone --depth 1 -c advice.detachedHead=false --branch v1.3.1 https://github.com/madler/zlib; \
    cd zlib; \
    mkdir build; \
    cd build; \
    cmake -G Ninja .. -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release"; \
    ../configure; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/libzstd; \
    cd /builds/debian-src-packages/libzstd; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y libzstd; \
    mkdir libzstd-src; \
    tar -xf libzstd_*.orig.tar.xz -C libzstd-src; \
    cd libzstd-src; \
    ZSTD_VERSION=$(ls | awk -F '-' '{print $2}'); \
    cd zstd-${ZSTD_VERSION}; \
    make -j${THREAD_COUNT}; \
    make install PREFIX=${SANITIZED_INSTALL_ROOT_DIR}; \
    mkdir -p /builds/debian-src-packages/bzip2; \
    cd /builds/debian-src-packages/bzip2; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y bzip2; \
    mkdir bzip2-src; \
    tar -zxf bzip2_*.orig.tar.gz -C bzip2-src; \
    cd bzip2-src; \
    BZIP2_VERSION=$(ls | awk -F '-' '{print $2}'); \
    cd bzip2-${BZIP2_VERSION}; \
    make -j${THREAD_COUNT} CC=$CC CFLAGS="$CFLAGS -Wall -Winline -O2 -D_FILE_OFFSET_BITS=64" LDFLAGS="$LDFLAGS"; \
    make install PREFIX=${SANITIZED_INSTALL_ROOT_DIR}; \
    cd ../..; \
    rm -rf bzip2-src; \
    mkdir bzip2-src; \
    tar -zxf bzip2_*.orig.tar.gz -C bzip2-src; \
    cd bzip2-src/bzip2-${BZIP2_VERSION}; \
    make -f Makefile-libbz2_so -j${THREAD_COUNT} CC=$CC CFLAGS="$CFLAGS -fpic -fPIC -Wall -Winline -O2 -D_FILE_OFFSET_BITS=64" LDFLAGS="$LDFLAGS"; \
    cp $(ls libbz2*.so*) ${SANITIZED_INSTALL_ROOT_DIR}/lib; \
    mkdir -p /builds/debian-src-packages/pcre2; \
    cd /builds/debian-src-packages/pcre2; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y pcre2; \
    mkdir pcre2-src; \
    tar -zxf $(ls pcre2_*.orig.tar.gz) -C pcre2-src; \
    cd pcre2-src; \
    PCRE2_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    cmake -G Ninja ../pcre2-${PCRE2_VERSION} -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release" -DPCRE2_SUPPORT_JIT="ON" -DPCRE2_SUPPORT_UNICODE="ON" -DBUILD_SHARED_LIBS="ON" -DPCRE2_BUILD_PCRE2_8="ON" -DPCRE2_BUILD_PCRE2_16="ON" -DPCRE2_BUILD_PCRE2_32="ON"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/brotli; \
    cd /builds/debian-src-packages/brotli; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y brotli; \
    mkdir brotli-src; \
    tar -zxf $(ls brotli_*.orig.tar.gz) -C brotli-src; \
    cd brotli-src; \
    BROTLI_VERSION=$(ls | awk -F '-' '{print $3}'); \
    mkdir build; \
    cd build; \
    cmake -G Ninja ../google-brotli-${BROTLI_VERSION} -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/draco; \
    cd /builds/debian-src-packages/draco; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y draco; \
    mkdir draco-src; \
    tar -xf $(ls draco_*.orig.tar.xz) -C draco-src; \
    cd draco-src; \
    DRACO_VERSION=$(ls | awk -F '-' '{print $3}'); \
    mkdir build; \
    cd build; \
    cmake -G Ninja ../google-draco-${DRACO_VERSION} -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release" -DBUILD_SHARED_LIBS="ON"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/pugixml; \
    cd /builds/debian-src-packages/pugixml; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y pugixml; \
    mkdir pugixml-src; \
    tar -zxf $(ls pugixml_*.orig.tar.gz) -C pugixml-src; \
    cd pugixml-src; \
    PUGIXML_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    cmake -G Ninja ../pugixml-${PUGIXML_VERSION} -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release" -DBUILD_SHARED_LIBS="ON"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/assimp; \
    cd /builds/debian-src-packages/assimp; \
    git clone --depth 1 -c advice.detachedHead=false --branch v5.4.3 https://github.com/assimp/assimp; \
    mkdir build; \
    cd build; \
    CXXFLAGS="-Wno-nontrivial-memcall $CXXFLAGS" cmake -G Ninja ../assimp -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release" -DASSIMP_BUILD_TESTS="OFF"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/openssl; \
    cd /builds/debian-src-packages/openssl; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y openssl; \
    mkdir openssl-src; \
    tar -zxf $(ls openssl_*.orig.tar.gz) -C openssl-src; \
    cd openssl-src; \
    OPENSSL_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    ../openssl-${OPENSSL_VERSION}/Configure --prefix=${SANITIZED_INSTALL_ROOT_DIR} --release -shared no-tests no-ssl3 no-comp; \
    make -j${THREAD_COUNT}; \
    make install; \
    hash -r; \
    mkdir -p /builds/debian-src-packages/icu; \
    cd /builds/debian-src-packages/icu; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y icu; \
    mkdir icu-src; \
    tar -zxf $(ls icu_*.orig.tar.gz) -C icu-src; \
    cd icu-src/icu; \
    mkdir build; \
    cd build; \
    ../source/configure --prefix=${SANITIZED_INSTALL_ROOT_DIR} --enable-release --enable-shared --with-data-packaging=library; \
    make -j${THREAD_COUNT}; \
    make install; \
    mkdir -p /builds/debian-src-packages/glib2.0; \
    cd /builds/debian-src-packages/glib2.0; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y glib2.0; \
    mkdir glib2.0-src; \
    tar -xf $(ls glib2.0_*.orig.tar.xz) -C glib2.0-src; \
    cd glib2.0-src; \
    GLIB2_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    CFLAGS="-Wno-implicit-function-declaration $CFLAGS" CXXFLAGS="-Qunused-arguments $CXXFLAGS" meson setup ../glib-${GLIB2_VERSION} -Dbuildtype='release' -Dcpp_stdlib='libc++' -Db_lundef='false' -Db_sanitize='thread' -Dwerror='false' --prefix=${SANITIZED_INSTALL_ROOT_DIR}; \
    meson compile; \
    meson install; \
    mkdir -p /builds/debian-src-packages/dbus; \
    cd /builds/debian-src-packages/dbus; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y dbus; \
    mkdir dbus-src; \
    tar -xf $(ls dbus_*.orig.tar.xz) -C dbus-src; \
    cd dbus-src; \
    DBUS_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    cmake -G Ninja ../dbus-${DBUS_VERSION} -DCMAKE_INSTALL_PREFIX="${SANITIZED_INSTALL_ROOT_DIR}" -DCMAKE_BUILD_TYPE="Release" -DDBUS_ENABLE_VERBOSE_MODE="OFF" -DDBUS_DISABLE_ASSERT="ON" -DDBUS_BUILD_TESTS="OFF"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    mkdir -p /builds/debian-src-packages/krb5; \
    cd /builds/debian-src-packages/krb5; \
    DEBIAN_FRONTEND=noninteractive apt-get -y install bison byacc; \
    DEBIAN_FRONTEND=noninteractive apt-get source -y krb5; \
    mkdir krb5-src; \
    tar -xzf $(ls krb5_*.orig.tar.gz) -C krb5-src; \
    cd krb5-src; \
    KERBEROS_VERSION=$(ls | awk -F '-' '{print $2}'); \
    mkdir build; \
    cd build; \
    ../krb5-${KERBEROS_VERSION}/src/configure --prefix=${SANITIZED_INSTALL_ROOT_DIR}; \
    make -j${THREAD_COUNT}; \
    make install; \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/)

# Build Qt
# We can install all dependencies required for building Qt by installing
# the build dependencies of Qt's dev packages. Qt's dev packages
# can be listed with the following command:
# apt-cache search qt6- | grep '^qt6.*' | grep dev
# And the dependencies can be installed as shown below:
# apt DEBIAN_FRONTEND=noninteractive apt-get -y build-dep qt6-base-dev qt6-httpserver-dev qt6-websockets-dev qt6-grpc-dev qt6-remoteobjects-dev
COPY --from=qt_resources qt_lldb_formatters.py /qt-resources/
COPY --from=qt_resources build-qt-module.sh /usr/local/bin/
RUN --mount=target=/var/lib/apt/lists,type=cache \
    --mount=target=/var/cache/apt,type=cache \
    --mount=target=/builds,type=cache \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/); \
    DEBIAN_FRONTEND=noninteractive apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y upgrade; \
    DEBIAN_FRONTEND=noninteractive apt-get -y build-dep qt6-base-dev qt6-httpserver-dev qt6-websockets-dev qt6-grpc-dev qt6-remoteobjects-dev qt6-networkauth-dev; \
    DEBIAN_FRONTEND=noninteractive apt-get -y install default-jdk maven python3-pip golang; \
    pip install --break-system-packages --root-user-action=ignore openapi-generator-cli; \
    export LD_LIBRARY_PATH="${LLVM_INSTALLATION_DIR}/lib/${LLVM_HOST_TARGET}:${LD_LIBRARY_PATH}"; \
    mkdir -p /builds/qt/qtbase; \
    cd /builds/qt/qtbase; \
    git clone --depth 1 -c advice.detachedHead=false --branch v${QT_VERSION} https://github.com/qt/qtbase; \
    mkdir build; \
    cd build; \
    ../qtbase/configure -prefix ${SANITIZED_INSTALL_ROOT_DIR} -platform linux-clang-libc++ -release -shared -force-bundled-libs -system-zlib -system-pcre -qt-sqlite -no-gui -no-widgets -nomake examples -nomake tests -nomake benchmarks -- -DCMAKE_CXX_STANDARD=20 -DCMAKE_JOB_POOLS:STRING=link_jobs=1 -DCMAKE_JOB_POOL_LINK:STRING=link_jobs; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    export Qt6_ROOT=/opt/install-root; \
    qt_modules=("qtwebsockets" "qthttpserver" "qtgrpc" "qtremoteobjects" "qttasktree" "qtopenapi" "qtnetworkauth"); \
    for i in "${qt_modules[@]}"; do build-qt-module.sh --thread-count ${THREAD_COUNT} --install-root-dir "${INSTALL_ROOT_DIR}" --qt-version ${QT_VERSION} --qt-module "$i"; done; \
    rm -rf $(ls -A /var/lib/apt/lists/); \
    rm -rf $(ls -A /var/cache/apt/); \
    rm -rf $(ls -A /builds/); \
    echo "command script import /qt-resources/qt_lldb_formatters.py" > /home/vscode/.lldbinit

ENV Qt6_ROOT=/opt/install-root
ENV TSAN_OPTIONS=report_bugs=1:exitcode=0:atexit_sleep_ms=1000:halt_on_error=0:die_after_fork=0

ENTRYPOINT ["/opt/source-env-vars.sh"]
CMD ["/bin/bash"]
