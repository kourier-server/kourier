+++
author = "Glauco Pacheco"
title = "C++ Kourier vs Rust Hyper vs Go http: Memory Consumption Benchmark"
subtitle = "4.7x less memory than Rust/Hyper, 7.7x less memory than Go/http, and a fully-compliant parser"
date = "2025-05-10"
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

In previous posts, HTTP performance and compliance were benchmarked. In this post, I benchmark memory consumption when the servers are subjected to a high load.

All assets used on benchmarks are container-based and publicly available and can be built with a simple command as follows:

```bash
git clone https://github.com/kourier-server/kourier.git Kourier
sudo ./Kourier/Src/Tests/Resources/Benchmarks/build_all.sh
```

As the results below indicate, Kourier consumes much less memory than Rust's Hyper and Go's http.

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

I use wrk to load the server. As I do not want to benchmark the network, I run the benchmarks over the localhost and split the available cores evenly between the servers and wrk. I use wrk from commit [1e6b9099](https://github.com/wg/wrk/commit/1e6b9099c999167873893c15cf99338267a7363e), which allows wrk to accept a set of IPs that it should bind to before connecting to the server. This feature enables us to create 500 K connections on localhost without exhausting the available ports:

```bash
# PORT is 3275 for Kourier, 8080 for Rust (Hyper), or 7080 for Go (net/http).
sudo docker run --rm --network host -it kourier-bench:wrk -c 500000 -d 60 -t 6 -b 127.0.0.0/8 --timeout 60s --latency http://localhost:PORT/hello
```

I set timers to exercise as much framework code as possible in the benchmark. On Kourier, I set request and idle timers; on Rust (Hyper), I set timers only for requests; on Go (net/http), I set request, idle, and write timers, as you can see on Kourier’s [repository](https://github.com/kourier-server/kourier) (all benchmark code is available at the *Src/Tests/Resources/Benchmarks* folder).<br><br>

# Results

```bash
# Testing Kourier server at 127.0.0.1:3275
glauco@ldh:~$ sudo docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=6 --request-timeout=20 --idle-timeout=60
d1606417573b4bbdb7645435b199eee4a6c4e6dd6ddf0b428bdee40fa3f7bb54
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:wrk -c 500000 -d 60 -t 6 -b 127.0.0.0/8 --timeout 60s --latency http://localhost:3275/hello
Running 1m test @ http://localhost:3275/hello
  6 threads and 500000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   909.75ms  300.77ms  27.83s    66.08%
    Req/Sec    42.39k    15.88k  111.69k    71.53%
  Latency Distribution
     50%  903.25ms
     75%    1.12s
     90%    1.31s
     99%    1.58s
  13898103 requests in 1.01m, 1.36GB read
Requests/sec: 229600.45
Transfer/sec:     22.99MB
glauco@ldh:~$ sudo docker inspect -f '{{.State.Pid}}' d16064
9284
glauco@ldh:~$ grep VmPeak /proc/9284/status
VmPeak:  1261344 kB
```

```bash
# Testing Rust (Hyper) server at 127.0.0.1:8080
glauco@ldh:~$ sudo docker run --rm -d --network host kourier-bench:rust-hyper -worker_count 6
ee93817a0bbde5a4f1815731bc7749bc0a464d1c845d61ba4f55896745d9fffd
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:wrk -c 500000 -d 60 -t 6 -b 127.0.0.0/8 --timeout 60s --latency http://localhost:8080/hello
Running 1m test @ http://localhost:8080/hello
  6 threads and 500000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.14s   157.73ms   7.90s    96.38%
    Req/Sec    37.80k     4.87k   47.44k    87.65%
  Latency Distribution
     50%    1.13s
     75%    1.14s
     90%    1.17s
     99%    1.54s
  12695817 requests in 1.01m, 1.22GB read
  Socket errors: connect 0, read 1, write 0, timeout 0
Requests/sec: 209752.03
Transfer/sec:     20.60MB
glauco@ldh:~$ sudo docker inspect -f '{{.State.Pid}}' ee93817
18104
glauco@ldh:~$ grep VmPeak /proc/18104/status
VmPeak:  5974808 kB
```

```bash
# Testing Go (net/http) server at 127.0.0.1:7080
glauco@ldh:~$ sudo docker run --rm -d --network host kourier-bench:go-net-http -worker_count 6
01dc6bd5097bb8e21ea74488d4e7fc6064073f3ea966f8609a64661e5a27e1a5
glauco@ldh:~$ sudo docker run --rm --network host -it kourier-bench:wrk -c 500000 -d 60 -t 6 -b 127.0.0.0/8 --timeout 60s --latency http://localhost:7080/hello
Running 1m test @ http://localhost:7080/hello
  6 threads and 500000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.69s   361.08ms   3.38s    79.32%
    Req/Sec    25.55k     8.42k   48.62k    73.07%
  Latency Distribution
     50%    1.57s
     75%    2.00s
     90%    2.07s
     99%    3.01s
  8575493 requests in 1.01m, 1.01GB read
Requests/sec: 141699.75
Transfer/sec:     17.03MB
glauco@ldh:~$ sudo docker inspect -f '{{.State.Pid}}' 01dc6bd
18492
glauco@ldh:~$ grep VmPeak /proc/18492/status
VmPeak:  9801832 kB
```

Besides being faster, Kourier consumes 4.7x less memory than Rust/Hyper and 7.7x less memory than Go/http.<br><br>

# Conclusion

Kourier is much faster than all publicly available servers tested, is the only server to pass all compliance tests, and consumes much less memory.

Kourier can empower the next generation of network appliances, enabling businesses that rely on them to run at a fraction of their infrastructure costs and in a much more HTTP-compliant manner.

You can [contact me](mailto:glaucopacheco@gmail.com) if your Business is not compatible with the requirements of the AGPL and you want to license Kourier under alternative terms.
