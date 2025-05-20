FROM kourier-bench:qt-base

COPY --from=kourier_src . /kourier-src/
COPY --from=certs_root certs /Kourier/certs/

ARG THREAD_COUNT

RUN mkdir -p /tmp/kourier-build; \
    cd /tmp/kourier-build; \
    cmake -DQt6_ROOT=/qt -DCMAKE_BUILD_TYPE=Release /kourier-src; \
    cmake --build . --target Lib --use-stderr -- -j ${THREAD_COUNT}; \
    cmake --install . --prefix "/kourier-lib"; \
    rm -rf /tmp/kourier-build; \
    mkdir -p /kourier-bench; \
    cd /kourier-bench; \
    cmake -DQt6_ROOT=/qt -DKourier_DIR=/kourier-lib/lib/cmake -DCMAKE_BUILD_TYPE=Release /kourier-src/Src/Tests/Resources/Benchmarks/Kourier/src; \
    cmake --build .

ENTRYPOINT ["/kourier-bench/kourier-bench"]
