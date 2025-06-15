package main

import (
	"time"
	"net/http"
	"runtime"
	"log"
	"flag"
	"fmt"
)

var helloWorld = []byte("Hello World!")


func helloHandler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Server", "Go")
    w.Header().Set("Content-Type", "text/plain")
    w.Write(helloWorld)
}

var worker_count = int(0)

func main() {
    flag.IntVar(&worker_count, "worker_count", -1, "Number of workers used by the runtime. Must be a positive integer.")
    flag.Parse()
    if worker_count <= 0 {
        panic("Server must be started with the worker_count command-line option indicating the number of threads to be used by the runtime (-worker_count N). The worker_count option value must be a positive integer.")
    } else {
	    fmt.Printf("Using %d workers.\n", worker_count)
   }
    runtime.GOMAXPROCS(worker_count)
    router := http.NewServeMux()
    router.HandleFunc("/", helloHandler)
    server := &http.Server{
	    Addr: "127.0.0.1:7080",
	    Handler: router,
	    ReadTimeout: 120 * time.Second,
	    IdleTimeout: 120 * time.Second,

    }
    err := server.ListenAndServe()
    if err != nil {
        log.Println(err)
        return
    }
}
