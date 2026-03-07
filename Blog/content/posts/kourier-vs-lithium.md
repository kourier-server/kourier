+++
author = "Glauco Pacheco"
title = "Kourier vs Lithium: The Fully-Compliant Server is Also the Fastest"
date = "2025-06-10"
authors = ["Glauco Pacheco"]
description = "Kourier is much faster and more compliant than Lithium"
tags = [
"kourier",
    "kourier",
    "lithium",
    "c++",
    "http",
    "server"
]
+++


<!--more-->

In this post, I benchmark Kourier against Lithium, one of TechEmpower's top performers. Both are written in C++ and open source.

As the benchmark results show, besides passing all compliance tests while Lithium fails all of them, Kourier is also faster than Lithium. Lithium fails all compliance tests because its parser only scans for CRLFs. In contrast, Kourier’s parser uses SIMD instructions extensively and can be faster while keeping strict adherence to HTTP’s syntax rules.

All benchmarks are reproducible, publicly available, open source, and container-based. All Docker images used for benchmarking can be easily built by running a [script](https://github.com/kourier-server/kourier/blob/main/Src/Tests/Resources/Benchmarks/build_all.sh) on Kourier's GitHub repository.

Unlike previous posts, this post uses an AWS EC2 instance to run the benchmarks. All benchmarks were run on an m7a.4xlarge instance running Debian Bookworm.

The instance is configured with the following commands:

```bash
# Installing Docker according to https://docs.docker.com/engine/install/debian/#install-using-the-repository
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install ca-certificates curl git
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/debian/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/debian \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
# Building benchmark assets
cd ~
git clone https://github.com/kourier-server/kourier.git Kourier
sudo ~/Kourier/Src/Tests/Resources/Benchmarks/build_all.sh
```

The servers are started with the following commands:

```bash
# Kourier. The server listens on port 3275
# and uses 8 threads to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=8
# Lithium. The server listens on port 8250
# and uses 8 threads to process incoming requests.
sudo docker run --rm -d --network host kourier-bench:lithium --worker-count=8
```

<br>

# HTTP Compliance Benchmark

In this benchmark, malformed requests containing invalid URL paths and queries, as well as invalid header field names and values, are sent to the servers. If the servers respond with a 200 OK HTTP status code, the respective test is considered as failed. The results are shown below:

| Server     | HTTP Compliance Score |
| -------   | :------: |
| Kourier    | 100.0% |
| Lithium | 0.0% |

```bash
# Testing Kourier server compliance at 127.0.0.1:3275
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
# Testing Lithium server compliance at 127.0.0.1:8250
sudo docker run --rm --network host -it kourier-bench:http-compliance -a 127.0.0.1 -p 8250
Testing HTTP compliance...
Finished testing HTTP compliance.
HTTP compliance score (% of passed tests):   0.0%
         0 requests with invalid URL absolute path detected.
       175 requests with invalid URL absolute path undetected.
         0 requests with invalid URL query detected.
       175 requests with invalid URL query undetected.
         0 requests with invalid header name detected.
       178 requests with invalid header name undetected.
         0 requests with invalid header value detected.
        30 requests with invalid header value undetected.
         0 requests with invalid pct-encoded hex digits in URL absolute path detected.
     61692 requests with invalid pct-encoded hex digits in URL absolute path undetected.
         0 requests with invalid pct-encoded hex digits in URL query detected.
     61692 requests with invalid pct-encoded hex digits in URL query undetected.
```

Kourier passes all compliance tests, and Lithium fails all of them. While Kourier's parser is fully HTTP syntax-compliant, Lithium's parser only scans for CRLFs and is unable to detect malformed requests.

Kourier has unmatched HTTP compliance. Implementing a fully compliant parser is a substantial task that requires meticulous design and development. For example,
let's run the tests for the *HttpRequestParser* class used by the Kourier HTTP server. The commands below were run on Debian Bookworm with clang-19 installed. The output of the DesignTests executable is rather long and prints all the scenarios specified in the [*HttpRequestParser.spec.cpp*](https://github.com/kourier-server/kourier/blob/main/Src/Http/HttpRequestParser.spec.cpp) file for the *HttpRequestParser* class. The *DesignTests* executable uses *Spectator*, a test framework I built exclusively for Kourier and that is open source and available on Kourier's repository (I had to write yet another test framework because all available alternatives for writing tests in the Gherkin style were unacceptably slow).

```bash
# Installing required dependencies
sudo apt-get install cmake ninja-build wget tar libssl-dev
# Installing clang-19 on Debian Bookworm
echo "deb http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm main" | sudo tee /etc/apt/sources.list.d/llvm.list > /dev/null
echo "deb-src http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm main" | sudo tee /etc/apt/sources.list.d/llvm-src.list > /dev/null
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo apt-get install clang-19
# Compiling Qt
qt_sdk_url="https://download.qt.io/official_releases/qt/6.9/6.9.0/submodules/qtbase-everywhere-src-6.9.0.tar.xz"
mkdir -p /tmp/qt
cd /tmp/qt
wget ${qt_sdk_url}
tar xf qtbase-everywhere-src-6.9.0.tar.xz
mv qtbase-everywhere-src-6.9.0 qt-src
mkdir -p /tmp/qt/qt-build
cd /tmp/qt/qt-build
../qt-src/configure -prefix ~/Qt -release -shared -no-gui -no-widgets -no-dbus -- -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_C_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_CXX_FLAGS="-march=native -fomit-frame-pointer -flto" -DCMAKE_SHARED_LINKER_FLAGS="-O3 -flto"
cmake --build . --parallel 16
cmake --install .
rm -rf /tmp/qt
# As Configured, Kourier repo was cloned at ~/Kourier
cd ~
mkdir Kourier-build
cd Kourier-build
cmake -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DQt6_ROOT=~/Qt -DCMAKE_BUILD_TYPE=Debug ../Kourier
cmake --build . --target DesignTests --use-stderr -- -j 16
./Src/Tests/Design/DesignTests -f HttpRequestParser.spec.cpp

------------------------------------------
           SUCCESSFUL TESTS (6282)
------------------------------------------

Scenario: HttpRequestParser processes all available request body when parsing request metadata (request line + headers)
  Given: a request with body
    When: request is parsed at once
      Then: parser processes body data available after request metadata is parsed
        And Then: parser extracts the correct information from the request data

Scenario: HttpRequestParser does not allow transfer-encoding trimmed values that do not end with chunked transfer-coding without parameters or weight
  Given: requests with transfer-encoding entries whose trimmed values end with chunked
    When: request is parsed at once
      Then: parser parses request
        And Then: parser extracts the correct information from request

Scenario: HttpRequestParser does not allow transfer-encoding trimmed values that do not end with chunked transfer-coding without parameters or weight
  Given: requests with transfer-encoding entries whose trimmed values end with chunked
    When: the request is parsed byte by byte
      Then: parser parses request
        And Then: parser extracts the correct information from request

Scenario: HttpRequestParser does not allow transfer-encoding trimmed values that do not end with chunked transfer-coding without parameters or weight
  Given: requests with transfer-encoding entries whose trimmed values do not end with chunked
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser does not allow transfer-encoding trimmed values that do not end with chunked transfer-coding without parameters or weight
  Given: requests with transfer-encoding entries whose trimmed values do not end with chunked
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: a single http request with only host header and no body
    When: request is parsed at once
      Then: request is successfully parsed
        And Then: parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: a single http request with only host header and no body
    When: the request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: multiple http requests with only host header and no body
    When: parser processes data from all requests at once
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: multiple http requests with only host header and no body
    When: parser processes data from all requests byte by byte
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: malformed requests lacking the host header
    When: request is parsed at once
      Then: parser fails to parse the malformed requests

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: malformed requests lacking the host header
    When: the request is parsed byte by byte
      Then: parser fails to parse the malformed requests

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests containing invalid methods
    When: request is parsed at once
      Then: parser fails to parse the malformed requests

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests containing invalid methods
    When: request is parsed byte by byte
      Then: parser fails to parse the malformed requests

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid characters in absolute path
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid characters in absolute path
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char as absolute path
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char as absolute path
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid percent-encoded hex chars in absolute path
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid percent-encoded hex chars in absolute path
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char in absolute path
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char in absolute path
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char as absolute path
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char as absolute path
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all invalid chars in absolute path
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all invalid chars in absolute path
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char in absolute path
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char in absolute path
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid characters in query
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid characters in query
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char as query
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char as query
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid percent-encoded hex chars in query
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all valid percent-encoded hex chars in query
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char in query
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing a percent-encoded hex char in query
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char as query
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char as query
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all invalid chars in query
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing all invalid chars in query
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char in query
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: request containing an invalid char in query
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with empty queries with only host header and no body
    When: the request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with empty queries with only host header and no body
    When: the request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with uncommon queries with only host header and no body
    When: the request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with uncommon queries with only host header and no body
    When: the request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with invalid http versions with only host header and no body
    When: the request is parsed at once
      Then: parser fails to parse the invalid request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with invalid http versions with only host header and no body
    When: the request is parsed byte by byte
      Then: parser fails to parse the invalid request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with invalid spaces with only host header and no body
    When: the request is parsed at once
      Then: parser fails to parse the invalid request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: requests with invalid spaces with only host header and no body
    When: the request is parsed byte by byte
      Then: parser fails to parse the invalid request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: invalid requests with invalid request lines with only the host header and no body
    When: the request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with only host header and no body
  Given: invalid requests with invalid request lines with only the host header and no body
    When: the request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser enforces limit on trailers
  Given: request with trailer name/value/line count larger than parser is allowed to accept is parsed
    When: request metadata is parsed at once
      Then: parser successfully parses request metadata
        And When: last chunk is parsed with trailers
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on trailers
  Given: request with trailer name/value/line count larger than parser is allowed to accept is parsed
    When: request is parsed byte by byte
      Then: parser successfully parses request metadata
        And When: last chunk is parsed with trailers
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser does not accept empty content-length values or values containing only spaces
  Given: a request with content-length header values that are empty or only containing spaces
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser does not accept empty content-length values or values containing only spaces
  Given: a request with content-length header values that are empty or only containing spaces
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser does not accept content-length values with more than 19 digits
  Given: a request with content-length header value larger than 19 digits
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser does not accept content-length values with more than 19 digits
  Given: a request with content-length header value larger than 19 digits
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser enforces limit on headers
  Given: request with header name/value/line count larger than parser is allowed to accept is parsed
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on headers
  Given: request with header name/value/line count larger than parser is allowed to accept is parsed
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser parses requests with chunked bodies
  Given: a single http request with headers and a chunked body
    When: the request is parsed at once
      Then: the request is successfully parsed
        And Then: the parser extracts the correct metadata from the request data
          And When: parser parses chunked body
            Then: parser successfully parses chunked body

Scenario: HttpRequestParser parses requests with chunked bodies
  Given: a single http request with headers and a chunked body
    When: the request is parsed byte by byte
      Then: request metadata (request line + headers) is successfully parsed
        And Then: parser extracts the correct metadata from request data
          And When: parser parses chunked body byte by byte
            Then: parser successfully parses chunked body

Scenario: HttpRequestParser parses requests with chunked bodies
  Given: multiple http requests with with headers and a chunked body
    When: parser processes data from all requests at once
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses requests with chunked bodies
  Given: multiple http requests with with headers and a chunked body
    When: parser processes data from all requests byte by byte
      Then: all requests are successfully parsed

Scenario: HttpRequestParser enforces limit on target uri
  Given: request with larger target uri is parsed
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on target uri
  Given: request with larger target uri is parsed
    When: request metadata is parsed byte by byte
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser parses http requests with headers and body
  Given: a single http request with headers and body
    When: the request is parsed at once
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with headers and body
  Given: a single http request with headers and body
    When: the request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with headers and body
  Given: multiple http requests with headers and body
    When: parser processes data from all requests at once
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses http requests with headers and body
  Given: multiple http requests with headers and body
    When: parser processes data from all requests byte by byte
      Then: all requests are successfully parsed

Scenario: HttpRequestParser allows spaces around content-length value
  Given: a request with content-length header value containing spaces before/after the value
    When: request is parsed at once
      Then: request is successfully parsed
        And Then: parser extracts the correct information from the request data

Scenario: HttpRequestParser allows spaces around content-length value
  Given: a request with content-length header value containing spaces before/after the value
    When: the request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser only accepts multiple content-length field lines when all of them have the same trimmed value
  Given: a request with multiple content-length field lines all with same value
    When: request is parsed at once
      Then: request is successfully parsed
        And Then: parser extracts the correct information from the request data

Scenario: HttpRequestParser only accepts multiple content-length field lines when all of them have the same trimmed value
  Given: a request with multiple content-length field lines all with same value
    When: the request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser only accepts multiple content-length field lines when all of them have the same trimmed value
  Given: a request containing multiple content-length entries with different trimmed values
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser only accepts multiple content-length field lines when all of them have the same trimmed value
  Given: a request containing multiple content-length entries with different trimmed values
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpServer responds with 100-Continue status code when client sends expect header with 100-continue value
  Given: a request containing an Expect: 100-continue header
    When: request metadata (request line + headers) is parsed at once
      Then: parser parses request metadata and writes 'HTTP/1.1 100 Continue\r\n\r\n' to io channel

Scenario: HttpServer responds with 100-Continue status code when client sends expect header with 100-continue value
  Given: a request containing an Expect: 100-continue header
    When: request metadata (request line + headers) is parsed byte by byte
      Then: parser parses request metadata and writes 'HTTP/1.1 100 Continue\r\n\r\n' to io channel

Scenario: HttpServer responds with 100-Continue status code when client sends expect header with 100-continue value
  Given: a request containing multiple Expect: 100-continue field lines in header
    When: request metadata (request line + headers) is parsed at once
      Then: parser parses request metadata and writes 'HTTP/1.1 100 Continue\r\n\r\n' to io channel

Scenario: HttpServer responds with 100-Continue status code when client sends expect header with 100-continue value
  Given: a request containing multiple Expect: 100-continue field lines in header
    When: request metadata (request line + headers) is parsed byte by byte
      Then: parser parses request metadata and writes 'HTTP/1.1 100 Continue\r\n\r\n' to io channel

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: a single http request with headers and no body
    When: the request is parsed at once
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: a single http request with headers and no body
    When: the request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: multiple http requests with headers and no body
    When: parser processes data from all requests at once
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: multiple http requests with headers and no body
    When: parser processes data from all requests byte by byte
      Then: all requests are successfully parsed

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests without header name
    When: the request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests without header name
    When: the request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: requests without header value
    When: the request is parsed at once
      Then: request is successfully parsed
        And Then: parser extracts correct information from request data

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: requests without header value
    When: the request is parsed byte by byte
      Then: request is successfully parsed
        And Then: parser extracts correct information from request data

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests lacking the host header field
    When: the request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests lacking the host header field
    When: the request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests with more than one host header field
    When: the request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: invalid requests with more than one host header field
    When: the request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all valid characters in header field name
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all valid characters in header field name
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char as header field name
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char as header field name
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all invalid chars in header field name
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all invalid chars in header field name
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char in header field name
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char in header field name
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all valid characters in header field value
    When: request is parsed at once
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all valid characters in header field value
    When: request is parsed byte by byte
      Then: parser parses the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char as header field value
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char as header field value
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all invalid chars in header field value
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing all invalid chars in header field value
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char in header field value
    When: request is parsed at once
      Then: parser fails to parse the request

Scenario: HttpRequestParser parses http requests with headers and no body
  Given: request containing an invalid char in header field value
    When: request is parsed byte by byte
      Then: parser fails to parse the request

Scenario: HttpRequestParser enforces limit on chunk metadata
  Given: request with chunk metadata larger than parser is allowed to accept is parsed
    When: request metadata is parsed at once
      Then: parser successfully parses request metadata
        And When: last chunk is parsed with trailers
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on chunk metadata
  Given: request with chunk metadata larger than parser is allowed to accept is parsed
    When: request is parsed byte by byte
      Then: parser successfully parses request metadata
        And When: last chunk is parsed with trailers
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: request with body larger than parser is allowed to parse
    When: request metadata is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: request with body larger than parser is allowed to parse
    When: request metadata is parsed byte by byte
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: chunked request with body larger than parser is allowed to parse
    When: request metadata is parsed at once
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: chunked request with body larger than parser is allowed to parse
    When: request metadata is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: chunked request with bodies whose sizes summed are larger than parser is allowed to parse
    When: request metadata is parsed at once
      Then: parser parses request metadata successfully
        And When: chunk is parsed
          Then: parser parses first chunk data
            And When: next chunk is parsed
              Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request body
  Given: chunked request with bodies whose sizes summed are larger than parser is allowed to parse
    When: request metadata is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: chunk is parsed
          Then: parser parses first chunk data successfully
            And When: next chunk is parsed
              Then: parser fails and reports too big message error

Scenario: HttpRequestParser does not allow requests containing both transfer-encoding and content-length header fields
  Given: requests containing both transfer-encoding and content-length
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser does not allow requests containing both transfer-encoding and content-length header fields
  Given: requests containing both transfer-encoding and content-length
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser does not allow multiple transfer-encoding entries
  Given: requests with transfer-encoding entries whose trimmed values end with chunked
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser does not allow multiple transfer-encoding entries
  Given: requests with transfer-encoding entries whose trimmed values end with chunked
    When: request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser allows server-wide options with * instead of absolute path for OPTIONS request
  Given: a server-wide OPTIONS request
    When: request is parsed at once
      Then: request is successfully parsed
        And Then: parser extracts the correct information from server-wide OPTIONS request

Scenario: HttpRequestParser allows server-wide options with * instead of absolute path for OPTIONS request
  Given: a server-wide OPTIONS request
    When: request is parsed byte by byte
      Then: the request is successfully parsed
        And Then: the parser extracts the correct information from the request data

Scenario: HttpRequestParser allows server-wide options with * instead of absolute path for OPTIONS request
  Given: a non-OPTIONS request targeting *
    When: request is parsed at once
      Then: parser fails to parse the malformed request

Scenario: HttpRequestParser allows server-wide options with * instead of absolute path for OPTIONS request
  Given: a non-OPTIONS request targeting *
    When: request is parsed byte by byte
      Then: parser fails to parse the malformed request

Scenario: HttpRequestParser only accepts digits in trimmed content-length value
  Given: a request with content-length header value containing trimmed value with non digit characters
    When: request is parsed at once
      Then: parser fails to parse request

Scenario: HttpRequestParser only accepts digits in trimmed content-length value
  Given: a request with content-length header value containing trimmed value with non digit characters
    When: the request is parsed byte by byte
      Then: parser fails to parse request

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing absolute path
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing absolute path
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing query
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing query
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing http version
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing http version
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing header name
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing header name
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing header value
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing header value
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded after parsing the headers and counting request body
    When: request is parsed at once
      Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded after parsing the headers and counting request body
    When: request is parsed byte by byte
      Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when parsing the chunk size
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when parsing the chunk size
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when parsing the chunk extension
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when parsing the chunk extension
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when adding the first chunk size to request size
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when adding the first chunk size to request size
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: chunk metadata is parsed
          Then: parser fails and reports too big message error before version gets parsed

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when adding the second chunk size to request size
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: first chunk is parsed
          Then: parser parses chunk
            And When: second chunk metadata is parsed
              Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a chunked request whose request size limit is exceeded when adding the second chunk size to request size
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: first chunk is parsed
          Then: parser parses chunk
            And When: second chunk metadata is parsed
              Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing trailer name
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: rest of chunked message is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing trailer name
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: rest of chunked message is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing trailer value
    When: request is parsed at once
      Then: parser parses request metadata successfully
        And When: rest of chunked message is parsed
          Then: parser fails and reports too big message error

Scenario: HttpRequestParser enforces limit on request size
  Given: a request whose request size limit is exceeded while parsing trailer value
    When: request is parsed byte by byte
      Then: parser parses request metadata successfully
        And When: rest of chunked message is parsed
          Then: parser fails and reports too big message error

------------------------------------------
           FAILED TESTS (0)
------------------------------------------


Total Scenarios :     21 (    21 Passed, 0 Failed)
Total Tests     :   6282 (  6282 Passed, 0 Failed)
Total Assertions: 320954 (320954 Passed, 0 Failed)
Time taken (ms) :    601
```

Kourier is thoroughly tested. All test source files in the Kourier repository end with *.spec.cpp* and use the Gherkin style to specify the expected behavior. There is a significant difference in testing rigor between Kourier and most open-source HTTP servers.<br><br>


# Performance Benchmark

| Server   |   RPS   |
| -------  | :------: |
| Kourier  | 33041886 |
| Lithium  | 32521508 |

This benchmark mimics TechEmpower's plaintext benchmark. Pipelined requests are sent to the servers through 512 connections to test how performant the frameworks are.

```bash
# Testing Kourier server at 127.0.0.1:3275
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 8 --latency http://localhost:3275/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ http://localhost:3275/hello
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.29ms    1.40ms  70.12ms   76.03%
    Req/Sec     4.16M    76.07k    4.30M    83.57%
  Latency Distribution
     50%    1.47ms
     75%    2.59ms
     90%    0.00us
     99%    0.00us
  498173184 requests in 15.08s, 48.72GB read
Requests/sec: 33041886.53
Transfer/sec:      3.23GB
```

```bash
# Testing Lithium server at 127.0.0.1:8250
sudo docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 8 --latency http://localhost:8250/hello -s /wrk/pipeline.lua -- 256
Running 15s test @ http://localhost:8250/hello
  8 threads and 512 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.45ms    4.04ms 119.06ms   86.25%
    Req/Sec     4.09M   247.53k    4.28M    98.67%
  Latency Distribution
     50%    1.37ms
     75%    1.99ms
     90%    0.00us
     99%    0.00us
  490560256 requests in 15.08s, 47.97GB read
Requests/sec: 32521508.53
Transfer/sec:      3.18GB
```

Even with its very simplified parser that only scans for CRLFs, Lithium cannot compete with Kourier's fully compliant parser, as Kourier's parser uses SIMD instructions extensively and can validate all bytes according to HTTP's syntax rules while being faster.<br><br>

# Conclusion

As the results above demonstrate, Kourier is faster than Lithium. Additionally, Kourier passes all HTTP compliance tests, whereas Lithium fails all of them, as Lithium's parser only scans for CRLFs and is thus unable to detect malformed requests.

With Kourier, you can create API servers at a fraction of your infrastructure cost. Kourier also supports request and idle timeouts and thus does not need to be hidden behind a reverse proxy.

Besides its ultra-fast server and parser, Kourier also provides classes for sockets, TLS encryption, and timers that can be used as building blocks in network appliances and firewalls that require extreme compliance and performance.

Being open-source, Kourier can be used, tested, verified, studied, and benchmarked freely. At https://kourier-server.github.io/docs, you can find detailed documentation for Kourier.

You can [contact me](mailto:glaucopacheco@gmail.com) if your Business is not compatible with the AGPL and you want to license Kourier under alternative terms.
