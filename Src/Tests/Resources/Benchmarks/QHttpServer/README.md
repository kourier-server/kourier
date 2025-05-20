Building Docker Container:
================================================================================
sudo docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. --build-context certs_root=.. -t kourier-bench:qhttpserver -f qhttpserver-bench.dockerfile .

Running Docker Container:
================================================================================
sudo docker run --rm --network host -d kourier-bench:qhttpserver -a 127.0.0.1 -p 5275
