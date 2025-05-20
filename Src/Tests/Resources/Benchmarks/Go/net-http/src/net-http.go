package main

import (
	"time"
	"net/http"
	"runtime"
	"log"
)

var helloWorld = []byte("Hello World!")


func helloHandler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Server", "Go")
    w.Header().Set("Content-Type", "text/plain")
    w.Write(helloWorld)
}

func main() {
    runtime.GOMAXPROCS(6)
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
