## SCM_RIGHTS

### This is a sample code for my Cloudflare blogpost about SCM_RIGHTS

Instructions:

Run `make`
Then launch `c_serv` and one of `rust_serv` or `go_serv`
Test using `echo passwhatever | nc localhost 8001` to get response from the secondary server or `echo anythingelse | nc localhost 8001` to get response from the C server