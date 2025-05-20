FROM debian:stable

ENV LD_LIBRARY_PATH=/openssl/lib64:/qt/lib
ENV LANG=C.UTF-8
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
ARG OPENSSL_VERSION
ARG THREAD_COUNT

RUN apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y upgrade; \
    DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential cmake wget ninja-build libssl-dev; \
    openssl_url="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"; \
    mkdir -p /tmp/openssl-build; \
    cd /tmp/openssl-build; \
    wget ${openssl_url}; \
    tar -zxf openssl-${OPENSSL_VERSION}.tar.gz; \
    cd openssl-${OPENSSL_VERSION}; \
    mkdir build; \
    cd build; \
    ../Configure --prefix=/openssl --release -shared no-tests no-ssl3 no-comp -fPIC -flto -Wl,-O3 -Wl,-flto -march=native; \
    make -j${THREAD_COUNT}; \
    make install; \
    hash -r; \
    rm -rf /tmp/openssl-build; \
    qt_sdk_url="https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/qtbase-everywhere-src-6.9.0.tar.xz"; \
    mkdir -p /tmp/qt; \
    cd /tmp/qt; \
    wget ${qt_sdk_url}; \
    tar xf qtbase-everywhere-src-6.9.0.tar.xz; \
    mv qtbase-everywhere-src-6.9.0 qt-src; \
    mkdir -p /tmp/qt/qt-build; \
    cd /tmp/qt/qt-build; \
    ../qt-src/configure -prefix /qt -release -shared -no-gui -no-widgets -no-dbus -- -DCMAKE_C_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_CXX_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_SHARED_LINKER_FLAGS="-O3 -flto"; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    rm -rf /tmp/qt

ENTRYPOINT ["/usr/bin/bash"]
