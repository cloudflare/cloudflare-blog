.PHONY: all

all: c_serv go_serv rust_serv

c_serv: ./c/main.c
	gcc  -o c_serv ./c/main.c

go_serv: ./go/main.go
	go build -o go_serv ./go/main.go

rust_serv: ./rust/scm_example/src/main.rs
	cargo build --manifest-path ./rust/scm_example/Cargo.toml
	mv ./rust/scm_example/target/debug/scm_example ./rust_serv

clean:
	rm c_serv go_serv rust_serv
	cargo clean --manifest-path ./rust/scm_example/Cargo.toml


