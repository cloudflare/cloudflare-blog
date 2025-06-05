# FinalizationRegistry Demo

Demo showcasing three WebAssembly memory management approaches in Cloudflare Workers:

- `/manual` - Manual memory management (call `.free()`)
- `/fr` - FinalizationRegistry (automatic GC cleanup)  
- `/explicit` - Explicit Resource Management (`using` statement)

## Setup

```bash
npm install
npm run start
```

Requires Rust with `wasm32-unknown-unknown` target.