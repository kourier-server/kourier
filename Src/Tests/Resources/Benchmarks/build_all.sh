#!/bin/bash
cd "$(dirname "$0")"
cd Go/net-http
docker build --no-cache -f net-http.dockerfile -t kourier-bench:go-net-http .
cd ../..
cd Rust/Hyper
docker build --no-cache -f hyper.dockerfile -t kourier-bench:rust-hyper .
cd ../..
cd QtBase
docker build --no-cache --build-arg THREAD_COUNT=12 --build-arg OPENSSL_VERSION=3.5.0 -t kourier-bench:qt-base -f qt-base.dockerfile .
cd ..
cd wrk
docker build --no-cache --build-arg THREAD_COUNT=12 -f wrk.dockerfile -t kourier-bench:wrk .
cd ..
cd Kourier
docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. --build-context certs_root=.. -t kourier-bench:kourier -f kourier.dockerfile .
cd ..
cd QHttpServer
docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. --build-context certs_root=.. -t kourier-bench:qhttpserver -f qhttpserver-bench.dockerfile .
cd ..
cd HttpCompliance
docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. -t kourier-bench:http-compliance -f http-compliance.dockerfile .
