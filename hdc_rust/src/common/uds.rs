/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//! uds
#![allow(missing_docs)]
#![allow(clippy::missing_safety_doc)]
use std::io::{self, ErrorKind, Result};
use std::mem;

use libc::bind;
use libc::c_void;
use libc::{accept4, close, connect};
use libc::{c_char, listen, poll, recv, socket, MSG_NOSIGNAL};
use libc::{c_int, sa_family_t, sockaddr, sockaddr_un, socklen_t, AF_UNIX};
use libc::{fcntl, pipe, read, send, socketpair, write, MSG_EOR};
use libc::{POLLERR, POLLHUP, POLLNVAL, POLLRDHUP};
use libc::{SOCK_CLOEXEC, SOCK_STREAM};

const LISTEN_BACKLOG: c_int = 10;
const MAX_CLIENT_FD_COUNT: usize = 256;

#[derive(PartialEq, Debug, Clone)]
pub struct PollNode {
    pub fd: i32,
    pub events: i16,
    pub revents: i16,
    pub ppid: u32,
    pub pkg_name: String,
    pub debug_or_release: bool,
}

impl PollNode {
    pub fn new(fd: i32, ppid: u32, pkg_name: String, debug_or_release: bool) -> Self {
        Self {
            fd,
            events: POLLNVAL | POLLRDHUP | POLLHUP | POLLERR,
            revents: 0,
            ppid,
            pkg_name,
            debug_or_release,
        }
    }
}

fn name_index() -> socklen_t {
    unsafe {
        let total_size = mem::size_of::<sockaddr_un>();
        let name_size = mem::size_of_val(&mem::zeroed::<sockaddr_un>().sun_path);
        (total_size - name_size) as socklen_t
    }
}

// const fn as_u8(slice: &[c_char]) -> &[u8] {
//     unsafe { &*(slice as *const [c_char] as *const [u8]) }
// }

const fn as_char(slice: &[u8]) -> &[c_char] {
    unsafe { &*(slice as *const [u8] as *const [c_char]) }
}

pub struct UdsAddr {
    addr: sockaddr_un,
    len: socklen_t,
}

impl Default for UdsAddr {
    fn default() -> Self {
        let mut addr: sockaddr_un = unsafe { mem::zeroed() };
        addr.sun_family = AF_UNIX as sa_family_t;
        Self {
            len: name_index(),
            addr,
        }
    }
}

impl UdsAddr {
    pub fn parse_abstract(name: &[u8]) -> io::Result<Self> {
        let mut addr = UdsAddr::default();
        addr.addr.sun_path[1..1 + name.len()].copy_from_slice(as_char(name));
        addr.len = name_index() + 1 + name.len() as socklen_t;
        Ok(addr)
    }

    pub fn as_raw_general(&self) -> (&sockaddr, socklen_t) {
        (
            unsafe { &*(&self.addr as *const sockaddr_un as *const sockaddr) },
            self.len,
        )
    }

    pub unsafe fn as_raw_mut_general(&mut self) -> (&mut sockaddr, &mut socklen_t) {
        (
            &mut *(&mut self.addr as *mut sockaddr_un as *mut sockaddr),
            &mut self.len,
        )
    }
}

pub struct UdsServer {}

impl UdsServer {
    pub fn wrap_socket(socket_type: c_int) -> i32 {
        let flags = socket_type | SOCK_CLOEXEC;
        unsafe { socket(AF_UNIX, flags, 0) }
    }

    pub fn wrap_bind(socket_fd: i32, addr: &UdsAddr) -> Result<()> {
        unsafe {
            let (addr_raw, len_raw) = addr.as_raw_general();
            loop {
                let ret = bind(socket_fd, addr_raw, len_raw);
                println!("bind ret : {}", ret);
                if ret != -1 {
                    break Ok(());
                }
                let err = io::Error::last_os_error();
                if err.kind() != ErrorKind::Interrupted {
                    break Err(err);
                }
            }
        }
    }

    pub fn wrap_listen(socket_fd: i32) -> c_int {
        unsafe { listen(socket_fd, LISTEN_BACKLOG) }
    }

    pub fn wrap_accept(socket_fd: i32) -> i32 {
        let mut addr = UdsAddr::default();
        let capacity = mem::size_of_val(&addr.addr) as socklen_t;
        addr.len = capacity;
        unsafe {
            let (addr_ptr, len_ptr) = addr.as_raw_mut_general();
            accept4(socket_fd, addr_ptr, len_ptr, SOCK_CLOEXEC)
        }
    }

    pub fn wrap_recv(socket_fd: i32, buffer: &mut [u8]) -> isize {
        let ptr = buffer.as_ptr() as *mut c_void;
        unsafe { recv(socket_fd, ptr, buffer.len(), MSG_NOSIGNAL) }
    }

    pub fn wrap_read(socket_fd: i32, buffer: &mut [u8]) -> isize {
        let ptr = buffer.as_ptr() as *mut c_void;
        unsafe { read(socket_fd, ptr, buffer.len()) }
    }

    pub fn wrap_write(socket_fd: i32, buffer: &[u8]) -> isize {
        let ptr = buffer.as_ptr() as *const c_void;
        unsafe { write(socket_fd, ptr, buffer.len()) }
    }

    pub fn wrap_poll(fds: &mut [PollNode], size: u32, timeout: i32) -> i32 {
        let init_value = unsafe { mem::zeroed() };
        let pollfds: &mut [libc::pollfd; MAX_CLIENT_FD_COUNT] =
            &mut [init_value; MAX_CLIENT_FD_COUNT];
        for (index, node) in fds.iter_mut().enumerate() {
            pollfds[index].fd = node.fd;
            pollfds[index].events = node.events;
            pollfds[index].revents = node.revents;
        }
        unsafe {
            let ret = poll(pollfds.as_mut_ptr(), size as libc::nfds_t, timeout);
            if ret == -1 {
                ret
            } else {
                for i in 0..size as usize {
                    fds[i].revents = pollfds[i].revents;
                    fds[i].events = pollfds[i].events;
                }
                0
            }
        }
    }

    #[allow(unused)]
    pub fn wrap_send(socket_fd: i32, buffer: &[u8]) -> isize {
        let ptr = buffer.as_ptr() as *const c_void;
        let flags = MSG_NOSIGNAL | MSG_EOR;
        unsafe { send(socket_fd, ptr, buffer.len(), flags) }
    }

    #[allow(unused)]
    pub fn wrap_close(socket_fd: i32) {
        unsafe { close(socket_fd) };
    }

    #[allow(unused)]
    pub fn wrap_socketpair(socket_type: c_int) -> Result<(i32, i32)> {
        let flags = socket_type | SOCK_CLOEXEC;
        let mut fd_buf = [-1; 2];
        unsafe {
            socketpair(AF_UNIX, flags, 0, fd_buf[..].as_mut_ptr());
            fcntl(fd_buf[1], 100, 20);
        }
        Ok((fd_buf[0], fd_buf[1]))
    }

    #[allow(unused)]
    pub fn wrap_pipe() -> Result<(i32, i32)> {
        let mut fd_buf = [-1; 2];
        unsafe {
            println!("pipe() begin...");
            let ret = pipe(fd_buf[..].as_mut_ptr());
            println!("pipe() ret:{}", ret);
            if ret >= 0 {
                fcntl(fd_buf[1], 100, 20);
            }
        }
        Ok((fd_buf[0], fd_buf[1]))
    }
}

pub struct UdsClient {}

impl UdsClient {
    pub fn wrap_socket(af: i32) -> i32 {
        let flags = SOCK_STREAM | SOCK_CLOEXEC;
        unsafe { socket(af, flags, 0) }
    }

    pub fn wrap_bind(socket_fd: i32, addr: &UdsAddr) -> Result<()> {
        unsafe {
            let (addr_raw, len_raw) = addr.as_raw_general();
            loop {
                let ret = bind(socket_fd, addr_raw, len_raw);
                println!("bind ret : {}", ret);
                if ret != -1 {
                    break Ok(());
                }
                let err = io::Error::last_os_error();
                if err.kind() != ErrorKind::Interrupted {
                    break Err(err);
                }
            }
        }
    }

    pub fn wrap_listen(socket_fd: i32) -> c_int {
        unsafe { listen(socket_fd, LISTEN_BACKLOG) }
    }

    pub fn wrap_connect(socket_fd: i32, addr: &UdsAddr) -> Result<()> {
        unsafe {
            let (addr_raw, len_raw) = addr.as_raw_general();
            println!("wrap_connect:len_raw: {:#?}", len_raw);
            println!("wrap_connect:addr_raw: {:#?}", addr_raw.sa_data);
            loop {
                let ret = connect(socket_fd, addr_raw, len_raw);
                println!("connect ret ++++++---->: {}", ret);
                if ret != -1 {
                    break Ok(());
                }
                let err = io::Error::last_os_error();
                if err.kind() != ErrorKind::Interrupted {
                    break Err(err);
                }
            }
        }
    }

    pub fn wrap_recv(socket_fd: i32, buffer: &mut [u8]) -> isize {
        let ptr = buffer.as_ptr() as *mut c_void;
        unsafe { recv(socket_fd, ptr, buffer.len(), MSG_NOSIGNAL) }
    }

    pub fn wrap_read(socket_fd: i32, buffer: &mut [u8]) -> isize {
        let ptr = buffer.as_ptr() as *mut c_void;
        unsafe { read(socket_fd, ptr, buffer.len()) }
    }

    pub fn wrap_write(socket_fd: i32, buffer: &[u8]) -> isize {
        let ptr = buffer.as_ptr() as *const c_void;
        unsafe { write(socket_fd, ptr, buffer.len()) }
    }

    pub fn wrap_send(socket_fd: i32, buffer: &[u8]) -> isize {
        let ptr = buffer.as_ptr() as *const c_void;
        let flags = MSG_NOSIGNAL | MSG_EOR;
        unsafe { send(socket_fd, ptr, buffer.len(), flags) }
    }
}
