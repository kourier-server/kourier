Building Docker Container:
================================================================================
docker build --no-cache --build-arg THREAD_COUNT=12 --build-context kourier_src=../../../../.. -t kourier-bench:http-compliance -f http-compliance.dockerfile .

Running Docker Container:
================================================================================
docker run --rm --network host -it kourier-bench:http-compliance -a IP -p PORT
