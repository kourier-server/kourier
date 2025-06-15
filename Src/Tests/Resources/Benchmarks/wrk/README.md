Building Docker Container:
================================================================================
docker build --no-cache --build-arg THREAD_COUNT=N -f wrk.dockerfile -t kourier-bench:wrk .

Running Docker Container:
================================================================================
For Performance tests: docker run --rm --network host -it kourier-bench:wrk -c 512 -d 15 -t 6 --latency http://localhost:PORT/hello -s /wrk/pipeline.lua -- 256

For memory consumption tests: docker run --rm --network host -it kourier-bench:wrk -c 500000 -d 60 -t 6 -b 127.0.0.0/8 --timeout 60s --latency http://localhost:PORT/hello

