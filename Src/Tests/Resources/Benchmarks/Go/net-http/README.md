Building Docker Container:
================================================================================
docker build --no-cache -f net-http.dockerfile -t kourier-bench:go-net-http .

Running Docker Container:
================================================================================
docker run --rm -d --network host kourier-bench:go-net-http -worker_count N (N is the number of threads used by the runtime and must be a positive integer)
