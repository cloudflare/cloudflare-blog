import rust_lib from './wasm/target/wasm32-unknown-unknown/debug/wasm.wasm';

const instance = await WebAssembly.instantiate(rust_lib);
const { memory, make_buffer, free_buffer } = instance.exports;

// Create a new WASM buffer by calling Rust and reading the returned length
// Returns { ptr, len } where ptr is the buffer pointer and len is the size
function createWasmBuffer() {
    const lenPtr = 0; // Use memory offset 0 as scratch space for length
    const ptr = make_buffer(lenPtr);
    // Read the buffer length that Rust wrote to memory
    const len = new DataView(memory.buffer).getUint32(lenPtr, true);
    return { ptr, len };
}

// Approach 1: Manual Memory Management
class ManualWasmBuffer {
    constructor(ptr, len) {
        this.ptr = ptr;
        this.len = len;
        this.data = new Uint8Array(memory.buffer, ptr, len);
    }

    free() {
        if (this.ptr !== null) {
            console.log("Manually freeing buffer");
            free_buffer(this.ptr, this.len);
            this.ptr = null;
        }
    }
}

// Approach 2: FinalizationRegistry (automatic cleanup when GC'd)
class FinalizationRegistryWasmBuffer {
    static #registry = new FinalizationRegistry((heldValue) => {
        console.log("FinalizationRegistry cleaning up buffer");
        free_buffer(heldValue.ptr, heldValue.len);
    });

    constructor(ptr, len) {
        this.ptr = ptr;
        this.len = len;
        this.data = new Uint8Array(memory.buffer, ptr, len);
        
        FinalizationRegistryWasmBuffer.#registry.register(this, { ptr, len });
    }
}

// Approach 3: Explicit Resource Management (using/Symbol.dispose)
class ExplicitResourceWasmBuffer {
    constructor(ptr, len) {
        this.ptr = ptr;
        this.len = len;
        this.data = new Uint8Array(memory.buffer, ptr, len);
    }

    [Symbol.dispose]() {
        if (this.ptr !== null) {
            console.log("Symbol.dispose cleaning up buffer");
            free_buffer(this.ptr, this.len);
            this.ptr = null;
        }
    }
}

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);
        const path = url.pathname;
        
        if (path === '/manual') {
            const { ptr, len } = createWasmBuffer();
			console.log(len);
            
            const buffer = new ManualWasmBuffer(ptr, len);
            const message = new TextDecoder().decode(buffer.data);
            
            buffer.free(); // Must manually free!
            
            return new Response(`Manual Memory Management: ${message.trim()}`);
        }
        
        if (path === '/fr') {
            const { ptr, len } = createWasmBuffer();
            
            let buffer = new FinalizationRegistryWasmBuffer(ptr, len);
            const message = new TextDecoder().decode(buffer.data);
            
            buffer = null; // Let GC and FinalizationRegistry handle cleanup
            
            return new Response(`FinalizationRegistry: ${message.trim()}`);
        }
        
        if (path === '/explicit') {
            const { ptr, len } = createWasmBuffer();
            
            using buffer = new ExplicitResourceWasmBuffer(ptr, len);
            const message = new TextDecoder().decode(buffer.data);
            
            return new Response(`Explicit Resource Management: ${message.trim()}`);
			// Symbol.dispose will be called automatically at scope exit
        }
        
        return new Response('Try: /manual, /fr, or /explicit');
    },
};