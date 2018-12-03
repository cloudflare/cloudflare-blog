extern crate libc;

use libc::*;
use std::io::prelude::*;
use std::net::TcpStream;
use std::os::unix::io::FromRawFd;
use std::os::unix::io::RawFd;

fn errno_str() -> String {
    let strerr = unsafe { strerror(*__error()) };
    let c_str = unsafe { std::ffi::CStr::from_ptr(strerr) };
    c_str.to_string_lossy().into_owned()
}

pub struct UNIXSocket {
    fd: RawFd,
}

pub struct UNIXConn {
    fd: RawFd,
}

impl Drop for UNIXSocket {
    fn drop(&mut self) {
        unsafe { close(self.fd) };
    }
}

impl Drop for UNIXConn {
    fn drop(&mut self) {
        unsafe { close(self.fd) };
    }
}

impl UNIXSocket {
    pub fn new() -> Result<UNIXSocket, String> {
        match unsafe { socket(AF_UNIX, SOCK_STREAM, 0) } {
            -1 => Err(errno_str()),
            fd @ _ => Ok(UNIXSocket { fd }),
        }
    }

    pub fn bind(self, address: &str) -> Result<UNIXSocket, String> {
        assert!(address.len() < 104);

        let mut addr = sockaddr_un {
            sun_len: std::mem::size_of::<sockaddr_un>() as u8,
            sun_family: AF_UNIX as u8,
            sun_path: [0; 104],
        };

        for (i, c) in address.chars().enumerate() {
            addr.sun_path[i] = c as i8;
        }

        match unsafe {
            unlink(&addr.sun_path as *const i8);
            bind(
                self.fd,
                &addr as *const sockaddr_un as *const sockaddr,
                std::mem::size_of::<sockaddr_un>() as u32,
            )
        } {
            -1 => Err(errno_str()),
            _ => Ok(self),
        }
    }

    pub fn listen(self) -> Result<UNIXSocket, String> {
        match unsafe { listen(self.fd, 50) } {
            -1 => Err(errno_str()),
            _ => Ok(self),
        }
    }

    pub fn accept(&self) -> Result<UNIXConn, String> {
        match unsafe { accept(self.fd, std::ptr::null_mut(), std::ptr::null_mut()) } {
            -1 => Err(errno_str()),
            fd @ _ => Ok(UNIXConn { fd }),
        }
    }
}

#[repr(C)]
pub struct ScmCmsgHeader {
    cmsg_len: c_uint,
    cmsg_level: c_int,
    cmsg_type: c_int,
    fd: c_int,
}

impl UNIXConn {
    pub fn recv_fd(&self) -> Result<RawFd, String> {
        let mut iov = iovec {
            iov_base: std::ptr::null_mut(),
            iov_len: 0,
        };

        let mut scm = ScmCmsgHeader {
            cmsg_len: 0,
            cmsg_level: 0,
            cmsg_type: 0,
            fd: 0,
        };

        let mut mhdr = msghdr {
            msg_name: std::ptr::null_mut(),
            msg_namelen: 0,
            msg_iov: &mut iov as *mut iovec,
            msg_iovlen: 1,
            msg_control: &mut scm as *mut ScmCmsgHeader as *mut c_void,
            msg_controllen: std::mem::size_of::<ScmCmsgHeader>() as u32,
            msg_flags: 0,
        };

        let n = unsafe { recvmsg(self.fd, &mut mhdr, 0) };

        if n == -1
            || scm.cmsg_len as usize != std::mem::size_of::<ScmCmsgHeader>()
            || scm.cmsg_level != SOL_SOCKET
            || scm.cmsg_type != SCM_RIGHTS
        {
            Err("Invalid SCM message".to_string())
        } else {
            Ok(scm.fd)
        }
    }
}

fn main() {
    let unix_sock = UNIXSocket::new()
        .and_then(|s| s.bind("/tmp/scm_example.sock"))
        .and_then(|s| s.listen())
        .unwrap();

    loop {
        let unix_conn = unix_sock.accept().unwrap();
        let mut tcp_conn = unsafe { TcpStream::from_raw_fd(unix_conn.recv_fd().unwrap()) };
        tcp_conn.write(b"Hello from Rust :)\n").unwrap();
    }
}
