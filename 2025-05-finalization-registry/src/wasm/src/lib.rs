// Allocate a fresh byte buffer and hand the raw pointer + length to JS.
// We intentionally “forget” the Vec so Rust will not free it right away;
// JS now owns it and must call `free_buffer` later.
#[no_mangle]
pub extern "C" fn make_buffer(out_len: *mut usize) -> *mut u8 {
    let mut data = b"Hello from Rust".to_vec();
    let ptr = data.as_mut_ptr();
    let len  = data.len();

    unsafe { *out_len = len };

    std::mem::forget(data);
    return ptr;
}

// Counterpart that **must** be called by JS to avoid a leak.
#[no_mangle]
pub unsafe extern "C" fn free_buffer(ptr: *mut u8, len: usize) {
    let _ = Vec::from_raw_parts(ptr, len, len);
}
