FROM kourier-bench:qt-base

COPY pipeline.lua /wrk/
ARG THREAD_COUNT

RUN apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get -y upgrade; \
    DEBIAN_FRONTEND=noninteractive apt-get -y install unzip git luajit2 libluajit2-5.1-dev; \
    wrk_url="https://github.com/wg/wrk/archive/1e6b9099c999167873893c15cf99338267a7363e.zip"; \
    mkdir -p /tmp/wrk-build; \
    cd /tmp/wrk-build; \
    wget ${wrk_url}; \
    unzip 1e6b9099c999167873893c15cf99338267a7363e.zip; \
    mv wrk-1e6b9099c999167873893c15cf99338267a7363e wrk-src; \
    cd wrk-src; \
    git config --global user.email "me@example.com"; \
    git config --global user.name "My Name"; \
    git init --initial-branch=main; \
    git add .; \
    git commit -m "Inital commit."; \
    make WITH_LUAJIT=/usr WITH_OPENSSL=/usr CFLAGS=" -std=c99 -Wall -fPIC -march=native -O3 -flto -D_REENTRANT -D_POSIX_C_SOURCE=200112L -D_BSD_SOURCE -D_DEFAULT_SOURCE -I/usr/include/luajit-2.1 -I/openssl/include " LDFLAGS=" -fPIC -march=native -O3 -flto -L/openssl/lib64 -Wl,-E " -j${THREAD_COUNT}; \
    mv ./wrk /usr/local/bin; \
    rm -rf /tmp/wrk-build

ENTRYPOINT ["/usr/local/bin/wrk"]
