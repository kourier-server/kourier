FROM kourier-bench:qt-base

COPY --from=kourier_src . /kourier-src/
COPY --from=certs_root certs /QHttpServer/certs/

ARG THREAD_COUNT

RUN qt_http_server_url="https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/qthttpserver-everywhere-src-6.9.0.tar.xz"; \
    mkdir -p /tmp/qt; \
    cd /tmp/qt; \
    wget ${qt_http_server_url}; \
    tar xf qthttpserver-everywhere-src-6.9.0.tar.xz; \
    mv qthttpserver-everywhere-src-6.9.0 qt-src; \
    mkdir -p /tmp/qt/qt-build; \
    cd /tmp/qt/qt-build; \
    cmake -DQt6_ROOT=/qt -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/qt -DCMAKE_C_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_CXX_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_SHARED_LINKER_FLAGS="-O3 -flto" /tmp/qt/qt-src; \
    cmake --build . --parallel ${THREAD_COUNT}; \
    cmake --install .; \
    rm -rf /tmp/qt; \
    mkdir -p /qhttpserver-bench; \
    cd /qhttpserver-bench; \
    cmake -DQt6_ROOT=/qt -DCMAKE_BUILD_TYPE=Release /kourier-src/Src/Tests/Resources/Benchmarks/QHttpServer; \
    cmake --build .

ENTRYPOINT ["/qhttpserver-bench/qhttpserver-bench"]
