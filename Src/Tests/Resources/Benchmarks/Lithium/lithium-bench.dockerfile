FROM kourier-bench:qt-base

COPY --from=kourier_src . /kourier-src/
# We use the commit a046b3345098157849d9e2ab49a475aaabf4a90f for Lithium header.
# This is the same commit used by Lithium developers on TechEmpower benchmarks.
# see https://github.com/TechEmpower/FrameworkBenchmarks/blob/master/frameworks/C%2B%2B/lithium/compile.sh

RUN DEBIAN_FRONTEND=noninteractive apt-get -y install libboost-context-dev libboost-dev; \
    cd /kourier-src/Src/Tests/Resources/Benchmarks/Lithium/src; \
    wget https://raw.githubusercontent.com/matt-42/lithium/a046b3345098157849d9e2ab49a475aaabf4a90f/single_headers/lithium_http_server.hh; \
    mkdir -p /lithium-bench; \
    cd /lithium-bench; \
    cmake -DQt6_ROOT=/qt -DCMAKE_BUILD_TYPE=Release /kourier-src/Src/Tests/Resources/Benchmarks/Lithium; \
    cmake --build .

ENTRYPOINT ["/lithium-bench/lithium-bench"]
