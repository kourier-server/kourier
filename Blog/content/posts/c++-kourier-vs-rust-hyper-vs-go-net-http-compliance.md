+++
author = "Glauco Pacheco"
title = "C++ Kourier vs Rust Hyper vs Go http: HTTP Compliance Benchmark"
date = "2025-05-12"
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

In other posts, I demonstrated that Kourier is much faster and consumes substantially less memory than Rust's Hyper and Go's net/http.

In this post, I compare the missing piece in server benchmarks, which is uncommon to see on the Internet - HTTP compliance benchmarks. Unfortunately, many servers and frameworks sacrifice HTTP conformance to simplify the implementation of their HTTP parsers.

In this post, I compare Kourier against Rust's Hyper and Go's net/http regarding compliance with HTTP syntax.

All Docker images used on the benchmarks were built with the following commands:

```bash
git clone https://github.com/kourier-server/kourier.git Kourier
sudo ./Kourier/Src/Tests/Resources/Benchmarks/build_all.sh
```

The results show that Kourier is, by far, the most compliant server.

The servers are started with the following commands:

```bash
# Kourier. The server listens on port 3275
sudo docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=1
# Rust (Hyper). The server listens on port 8080
sudo docker run --rm -d --network host kourier-bench:rust-hyper -worker_count 1
# Go (net/http). The server listens on port 7080
sudo docker run --rm -d --network host kourier-bench:go-net-http -worker_count 1
```

The *kourier-bench:http-compliance* Docker image is used to test compliance with HTTP syntax. This image generates malformed requests containing invalid characters for the URL's absolute path, query, and header field names and values. If the server responds with an HTTP 200 status code, the test is considered as failed.


In Kourier's [repository](https://github.com/kourier-server/kourier), you can check the source file *Src/Tests/Resources/Benchmarks/HttpCompliance/src/main.cpp* to see how the requests are generated.

The client that tests the server's compliance is started with the following command:

```bash
# PORT is 3275 for Kourier, 8080 for Rust (Hyper), or 7080 for Go (net/http).
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p PORT
```

<br>

# Results

| Server     | HTTP Compliance Score |
| -------   | :------: |
| Kourier    | 100.0% |
| Rust Hyper | 89.7% |
| Go http    | 63.1% |

```bash
# Testing Kourier server at 127.0.0.1:3275
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 3275
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests): 100.0%
       175 requests with invalid URL absolute path detected.
         0 requests with invalid URL absolute path undetected.
       175 requests with invalid URL query detected.
         0 requests with invalid URL query undetected.
       178 requests with invalid header name detected.
         0 requests with invalid header name undetected.
        30 requests with invalid header value detected.
         0 requests with invalid header value undetected.
     61692 requests with invalid pct-encoded hex digits in URL absolute path detected.
         0 requests with invalid pct-encoded hex digits in URL absolute path undetected.
     61692 requests with invalid pct-encoded hex digits in URL query detected.
         0 requests with invalid pct-encoded hex digits in URL query undetected.
```

```bash
# Testing Rust (Hyper) server at 127.0.0.1:8080
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 8080
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests):  89.7%
       165 requests with invalid URL absolute path detected.
        10 requests with invalid URL absolute path undetected.
       165 requests with invalid URL query detected.
        10 requests with invalid URL query undetected.
       178 requests with invalid header name detected.
         0 requests with invalid header name undetected.
        30 requests with invalid header value detected.
         0 requests with invalid header value undetected.
     55332 requests with invalid pct-encoded hex digits in URL absolute path detected.
      6360 requests with invalid pct-encoded hex digits in URL absolute path undetected.
     55332 requests with invalid pct-encoded hex digits in URL query detected.
      6360 requests with invalid pct-encoded hex digits in URL query undetected.
```

```bash
# Testing Go (net/http) server at 127.0.0.1:7080
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 7080
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests):  63.1%
        35 requests with invalid URL absolute path detected.
       140 requests with invalid URL absolute path undetected.
        34 requests with invalid URL query detected.
       141 requests with invalid URL query undetected.
       177 requests with invalid header name detected.
         1 requests with invalid header name undetected.
        30 requests with invalid header value detected.
         0 requests with invalid header value undetected.
     61692 requests with invalid pct-encoded hex digits in URL absolute path detected.
         0 requests with invalid pct-encoded hex digits in URL absolute path undetected.
     16252 requests with invalid pct-encoded hex digits in URL query detected.
     45440 requests with invalid pct-encoded hex digits in URL query undetected.
```

<br>

# Conclusion

Besides being faster than all publicly available servers, Kourier is also 100% HTTP syntax-compliant. Kourier is much more compliant than popular alternatives for writing API servers, as the benchmark results show.

Aside from providing a server with unbeatable performance and compliance, Kourier also exports classes for TCP and TLS-encrypted sockets, as well as C++ classes for lightweight timers and a modern signal-slot implementation used by Kourier itself. You can use the Kourier server and the classes it provides to implement fully HTTP-compliant, high-performance systems. At https://kourier-server.github.io/docs, you can find detailed documentation for Kourier.

Kourier is open-source and can be used, tested, benchmarked, and analyzed freely. With Kourier, you can implement API servers at a fraction of the infrastructure cost required by the alternatives. Kourier can be used where extreme performance and compliance are indispensable, such as for manufacturers of network appliances and firewalls.

You can [contact me](mailto:glaucopacheco@gmail.com) if your Business is not compatible with the requirements of the AGPL and you want to license Kourier under alternative terms.
