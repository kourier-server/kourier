FROM kourier-bench:qt-base

COPY --from=kourier_src . /kourier-src/

ARG THREAD_COUNT

RUN mkdir -p /http-compliance-bench; \
    cd /http-compliance-bench; \
    cmake -DQt6_ROOT=/qt -DCMAKE_BUILD_TYPE=Release /kourier-src/Src/Tests/Resources/Benchmarks/HttpCompliance; \
    cmake --build .

ENTRYPOINT ["/http-compliance-bench/http-compliance-bench"]
