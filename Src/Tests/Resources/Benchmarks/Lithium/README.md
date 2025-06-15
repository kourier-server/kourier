Building Docker Container:
================================================================================
sudo docker build --no-cache --build-context kourier_src=../../../../.. -t kourier-bench:lithium -f lithium-bench.dockerfile .

Running Docker Container:
================================================================================
sudo docker run --rm --network host -d kourier-bench:lithium --worker-count=N (N is the number of threads used by the runtime and must be a positive integer)
