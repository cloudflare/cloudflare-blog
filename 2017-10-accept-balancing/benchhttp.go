package main

import (
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"math/rand"
	"net"
	"net/http"
	"os"
	"sync"
	"time"
	"crypto/tls"
)

func singleThread(ch chan string, wg *sync.WaitGroup) {
	dialer := &net.Dialer{}

	dial := func(ctx context.Context, network, addr string) (net.Conn, error) {
		if resolve != "" {
			addr = resolve
		}
		return dialer.DialContext(ctx, network, addr)
	}

	tr := &http.Transport{
		IdleConnTimeout:     30 * time.Second,
		DisableCompression:  true,
		DisableKeepAlives:   keepalive == 0,
		DialContext:         dial,
		MaxIdleConns:        0,
		MaxIdleConnsPerHost: 0,
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}
	client := &http.Client{Transport: tr}

	for url := range ch {
		t0 := time.Now()
		resp, err := client.Get(url)
		if err != nil {
			fmt.Fprintf(os.Stderr, "[!] %q %s\n", url, err)
			continue
		}

		if keepalive > 0 && rand.Int31n(int32(keepalive)) == 0 {
			resp.Close = true
		}

		_, err = ioutil.ReadAll(resp.Body)
		if err != nil {
			fmt.Fprintf(os.Stderr, "[!] %q %s\n", url, err)
			continue
		}
		t1 := time.Now()

		td := float64(t1.Sub(t0).Nanoseconds())
		fmt.Printf("%.9f %d\n", td/1000000., resp.StatusCode)
	}

	wg.Done()
}

func main() {
	flag.Parse()
	if len(flag.Args()) == 0 {
		fmt.Fprintf(os.Stderr, "[*] Pass a url please\n")
		os.Exit(1)
	}

	fmt.Fprintf(os.Stderr, "time_in_ms status_code\n")

	ch := make(chan string, 512)
	wg := &sync.WaitGroup{}

	for i := 0; i < concurrency; i++ {
		wg.Add(1)
		go singleThread(ch, wg)
	}

	for {
		for _, s := range flag.Args() {
			number -= 1
			if number < 0 {
				goto after
			}
			ch <- s
		}
	}

after:
	close(ch)
	wg.Wait()
}

var (
	number      int
	concurrency int
	keepalive   int
	resolve     string
)

func init() {
	flag.IntVar(&number, "n", 100, "Number of requests")
	flag.IntVar(&concurrency, "c", 1, "Concurrency")
	flag.IntVar(&keepalive, "k", 0, "Enable keepalive")
	flag.StringVar(&resolve, "r", "", "Resolve IP to this address")
}
