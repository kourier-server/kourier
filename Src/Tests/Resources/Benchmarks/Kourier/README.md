Building Docker Container:
================================================================================
docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. --build-context certs_root=.. -t kourier-bench:kourier -f kourier.dockerfile .

Running Docker Container:
================================================================================
docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=6 --request-timeout=20 --idle-timeout=60

docker run --rm -d --network host kourier-bench:kourier -a 127.0.0.1 -p 3275 --worker-count=6 --enable-tls --request-timeout=20 --idle-timeout=60
