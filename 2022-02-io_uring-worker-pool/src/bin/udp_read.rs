// Copyright (c) 2022 Cloudflare, Inc.

use anyhow::{anyhow, Result};
use io_uring::{
    cqueue::CompletionQueue,
    opcode,
    squeue::{self, Entry, SubmissionQueue},
    types, IoUring,
};
use nix::{
    sched::{self, CpuSet},
    unistd::Pid,
};
use std::{io::ErrorKind, net::UdpSocket, os::unix::io::AsRawFd, thread};
use structopt::StructOpt;
use libc;

const MAX_SQES: u32 = 4096;

#[derive(Debug, StructOpt)]
#[structopt(name = "udp-read", about = "read from UDP socket with io_uring")]
struct Opts {
    /// Set IOSQE_ASYNC flag on submitted SQEs
    #[structopt(short = "a", long = "async")]
    async_work: bool,

    /// Number of read requests to submit per io_uring (0 - fill the whole queue)
    #[structopt(short = "s", long, default_value = "0")]
    sqes: u32,

    /// Maxium number of unbound workers per NUMA node (0 - default, that is RLIMIT_NPROC)
    #[structopt(short = "w", long = "workers", default_value = "0")]
    max_unbound_workers: u32,

    /// Number io_ring instances to create per thread
    #[structopt(short = "r", long = "rings", default_value = "1")]
    num_rings: usize,

    /// Number of threads creating io_uring instances
    #[structopt(short = "t", long = "threads", default_value = "1")]
    num_threads: usize,

    /// CPU to run on when invoking io_uring_enter for Nth ring (specify multiple times)
    #[structopt(short = "c", long = "cpu", default_value = "0")]
    cpu: Vec<usize>,
}

fn main() -> Result<()> {
    let opts = Opts::from_args();

    if opts.num_threads > 1 {
        let threads: Vec<_> = (0..opts.num_threads)
            .map(|_| thread::spawn(do_work))
            .collect();
        for t in threads {
            t.join().unwrap()?;
        }
    } else {
        // Don't spawn any threads when we want just one.
        // This makes RLIMIT_NPROC experiments possible.
        do_work()?
    }
    Ok(())
}

fn do_work() -> Result<()> {
    let opts = Opts::from_args();

    let sink = UdpSocket::bind("127.0.0.1:0")?;
    let sink_fd = types::Fd(sink.as_raw_fd());

    let mut rd_buf = [0u8; 1];
    let rd_op = opcode::Read::new(sink_fd, &mut rd_buf as _, rd_buf.len() as _);
    let rd_flags = if opts.async_work {
        squeue::Flags::ASYNC
    } else {
        squeue::Flags::empty()
    };
    let rd_sqe = rd_op.build().flags(rd_flags);

    let mut cpu_iter = opts
        .cpu
        .iter()
        .map(|c| {
            let mut cs = CpuSet::new();
            cs.set(*c).unwrap();
            cs
        })
        .cycle();

    // (0) Create io_uring
    let mut rings: Vec<IoUring> = Vec::with_capacity(opts.num_rings);
    for _ in 0..opts.num_rings {
        let r = create_ring(MAX_SQES, opts.max_unbound_workers)?;
        rings.push(r);
    }

    // Save CPU affinity
    let cpu_set = sched::sched_getaffinity(Pid::from_raw(0))?;

    // (1) Fill submission queue just once
    let num_sqes = if opts.sqes > 0 { opts.sqes } else { MAX_SQES };
    for r in &mut rings {
        let c = cpu_iter.next().unwrap();
        sched::sched_setaffinity(Pid::from_raw(0), &c)?;

        fill_sq(&mut r.submission(), &rd_sqe, num_sqes)?;
        r.submit()?;
    }

    // Restore CPU affinity
    sched::sched_setaffinity(Pid::from_raw(0), &cpu_set)?;

    loop {
        // (2) Wait for at least 1 request to complete
        for r in &rings {
            'restart: loop {
                match r.submit_and_wait(1) {
                    Err(e) if e.kind() == ErrorKind::Interrupted => continue 'restart,
                    Err(e) => return Err(anyhow!(e)),
                    Ok(_) => break,
                }
            }
        }

        // (3) Drain completion queue
        for r in &mut rings {
            drain_cq(&mut r.completion())?;
        }
    }
}

fn create_ring(max_sqes: u32, max_unbound_workers: u32) -> Result<IoUring> {
    let ring = IoUring::new(max_sqes)?;
    let sub = ring.submitter();

    let mut max_workers: [u32; 2] = [0, max_unbound_workers];
    sub.register_iowq_max_workers(&mut max_workers)?;

    Ok(ring)
}

fn fill_sq(sq: &mut SubmissionQueue, sqe: &Entry, num_sqes: u32) -> Result<()> {
    let mut i = 0;

    sq.sync(); // load sq->head
    while !sq.is_full() && i < num_sqes {
        unsafe {
            sq.push(sqe)?;
        }
        i += 1;
    }
    sq.sync(); // store sq->tail

    Ok(())
}

fn drain_cq(cq: &mut CompletionQueue) -> Result<()> {
    cq.sync(); // load cq->tail
    for cqe in cq.into_iter() {
        let r = cqe.result();
        assert!(
            r == 1 || r == -libc::EINTR,
            "error: request completed with unexpected result {}",
            r
        );
    }
    cq.sync(); // store cq->head

    Ok(())
}
