+++
author = "Glauco Pacheco"
title = "C++ Kourier vs Rust Hyper vs Go http: Performance Benchmark"
subtitle = "12.1M RPS on an AMD Ryzen 5 1600, 52x Faster than Go, and a fully-compliant parser"
date = "2025-05-13"
authors = ["Glauco Pacheco"]
description = "Kourier leaves Rust Hyper and Go net/http in the dust"
tags = [
    "c++",
    "http",
    "server",
    "kourier",
    "rust",
    "hyper",
    "go",
    "golang"
]
+++


<!--more-->

In this post, I benchmark the performance of Kourier against Rust's Hyper and Go's net/http.

Kourier is open-source, and all the assets used in the benchmarks are publicly available and container-based, allowing anyone to easily reproduce them locally. All Docker images used on the benchmarks were built as follows:

```bash
git clone https://github.com/kourier-server/kourier.git Kourier
sudo ./Kourier/Src/Tests/Resources/Benchmarks/build_all.sh
```

The results show that Kourier is a performance powerhouse, capable of processing 12.1 million HTTP requests per second on an AMD Ryzen 5 1600, an 8-year-old mid-range processor, using only half of its cores (wrk uses the other half). The results set a new standard for HTTP servers, leaving the highest-performing frameworks far behind.

Kourier relies on AVX2 to parse HTTP requests, and AMD's Zen architecture is known for its lackluster AVX performance. In another post, a modern AMD EPYC-based AWS EC2 instance is used to benchmark Kourier against Lithium, the fastest C++ server on TechEmpower plaintext benchmark and one of TechEmpower's top performers.

I use an approach based on TechEmpower's plaintext benchmark. "Hello World" HTTP benchmarks are relevant because a lot of code is exercised between receiving a network data payload and calling a registered HTTP handler, and it is that code that I want to benchmark.

Although Kourier leaves all publicly available servers far behind, many frameworks sacrifice HTTP conformance to make it easier to implement their HTTP parser, which has the side effect of making them faster. In another post, I show that Kourier is much more HTTP syntax-compliant than Rust/Hyper and Go/http.

I also benchmarked, in another post, the memory consumption when the servers are put under high load and showed that Kourier consumes significantly less memory than Rust/Hyper and Go/http.

The servers are started with the following commands:

```bash
# Kourier. The server listens on port 3275
# and uses six threads to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=6 --request-timeout=20 --idle-timeout=60
# Rust (Hyper). The server listens on port 8080
# and uses six threads to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:rust-hyper -worker_count 6
# Go (net/http). The server listens on port 7080
# and uses six threads to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:go-net-http -worker_count 6
```

I use wrk to load the server. As I do not want to benchmark the network, I run the benchmarks over the localhost and split the available cores evenly between the servers and wrk. I use the following command to make wrk use 512 connections over six threads during 15 seconds to load the server with pipelined requests:

```bash
# PORT is 3275 for Kourier, 8080 for Rust (Hyper), or 7080 for Go (net/http).
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency http://localhost:PORT/hello -s /wrk/pipeline.lua -- 256
```

I set timers to exercise as much framework code as possible in the benchmark. On Kourier, I set timeouts for request processing and idle connections; on Rust (Hyper), I set timeouts for requests only; on Go (net/http), I set request, idle, and write timeouts, as you can see on Kourier’s [repository](https://github.com/kourier-server/kourier) (all benchmark code is available at the *Src/Tests/Resources/Benchmarks* folder).<br><br>

# Results

```bash
# Testing Kourier server at 127.0.0.1:3275
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency http://localhost:3275/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ http://localhost:3275/hello
  6 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.41ms    3.05ms  60.70ms   65.46%
    Req/Sec     2.04M   104.69k    2.44M    82.17%
  Latency Distribution
     50%    4.23ms
     75%    6.91ms
     90%    9.57ms
     99%    0.00us
  182933248 requests in 15.07s, 17.89GB read
Requests/sec: 12138068.83
Transfer/sec:      1.19GB
```

```bash
# Testing Rust (Hyper) server at 127.0.0.1:8080
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency http://localhost:8080/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ http://localhost:8080/hello
  6 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    24.34ms   19.22ms 103.72ms   65.81%
    Req/Sec   737.44k    45.53k    0.97M    76.00%
  Latency Distribution
     50%   19.89ms
     75%   37.51ms
     90%   55.08ms
     99%   80.51ms
  66036480 requests in 15.07s, 6.33GB read
Requests/sec: 4382914.88
Transfer/sec:    430.53MB
```

```bash
# Testing Go (net/http) server at 127.0.0.1:7080
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency http://localhost:7080/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ http://localhost:7080/hello
  6 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   491.73ms  413.30ms   2.00s    67.97%
    Req/Sec    38.89k    10.21k  111.96k    70.41%
  Latency Distribution
     50%  402.08ms
     75%  742.12ms
     90%    1.15s
     99%    1.81s
  3480465 requests in 15.06s, 418.22MB read
  Socket errors: connect 0, read 0, write 0, timeout 722
Requests/sec: 231080.00
Transfer/sec:     27.77MB
```

<br>

# Conclusion

Kourier is a performance powerhouse that leaves even top-performer servers behind.

Beating the fastest servers requires much more than a stellar HTTP parser. From custom timer implementation to efficient usage of epoll and high-performance ring buffers. Besides a super-fast server, Kourier exports classes for TCP and TLS-encrypted sockets, timers, and an efficient signal-slot mechanism that can be used as building blocks for code requiring extreme network performance. At https://kourier-server.github.io/docs, you can find detailed documentation for Kourier and all the classes it exports.

I developed Kourier with strict and demanding requirements, where all possible behaviors are comprehensively verified in specifications written in the Gherkin style. To this end, I created Spectator, a test framework that I also open-sourced with Kourier. You can check all files ending in spec.cpp in the Kourier repository to see how meticulously tested Kourier is. There is a stark difference in testing rigor between Kourier and other frameworks.

Kourier can empower the next generation of network appliances, enabling businesses that rely on them to run at a fraction of their infrastructure costs and in a much more HTTP-compliant way.

You can [contact me](mailto:glaucopacheco@gmail.com) if your Business is not compatible with the requirements of the AGPL and you want to license Kourier under alternative terms.
