Building Docker Container:
================================================================================
docker build --no-cache -f hyper.dockerfile -t kourier-bench:rust-hyper .

Running Docker Container:
================================================================================
docker run --rm -d --network host kourier-bench:rust-hyper -worker_count N (N is the number of threads used by the runtime and must be a positive integer)
