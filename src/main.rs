// Copyright 2018 Matthias P. Braendli
// SPDX-License-Identifier: GPL-2.0-only
extern crate libc;

use std::fs::{OpenOptions, File};
use std::ffi::{CStr, CString};
use std::os::unix::io::AsRawFd;
use std::thread;
use std::net::TcpStream;
use std::str::FromStr;
use std::io::{Read, Write};

extern {
    fn kissattach(
        callsign: * const libc::c_char,
        speed: libc::int32_t,
        mtu: libc::int32_t,
        kttyname: * const libc::c_char,
        allow_broadcast: libc::int32_t) -> libc::int32_t;
}

fn create_pts_pair() -> std::io::Result<File> {
    let master_file = OpenOptions::new()
        .read(true)
        .write(true)
        .open("/dev/ptmx")?;

    unsafe {
        let master_fd = master_file.as_raw_fd();
        if libc::grantpt(master_fd) == -1 {
            return Err(std::io::Error::last_os_error());
        }
        if libc::unlockpt(master_fd) == -1 {
            return Err(std::io::Error::last_os_error());
        }
    }

    Ok(master_file)
}

fn usage() {
    eprintln!("fldigi-kissattach");
    eprintln!(" Usage:");
    eprintln!(" fldigi-kissattach CALLSIGN-SSID FLDIGI_IP:FLDIGI_PORT MTU");
    eprintln!("");
    eprintln!(" Example:");
    eprintln!(" fldigi-kissattach HB9EGM-1 127.0.0.1:7342 120");

    std::process::exit(1);
}

fn main() {
    let args : Vec<String> = std::env::args().collect();
    if args.len() != 4 {
        usage();
    }

    let callsign = args[1].clone();
    let fldigi_connect = args[2].clone();
    let mtu = i32::from_str(&args[3]).unwrap();
    eprintln!("Creating PTY pair");

    let mut master_file = match create_pts_pair() {
        Ok(fd) => fd,
        Err(e) => panic!("create_pts_pair failed: {}", e)
    };

    eprintln!("PTS master: {:?}", master_file);

    let slavename;
    unsafe {
        slavename = libc::ptsname(master_file.as_raw_fd());
    }

    if slavename.is_null() {
        panic!("Cannot get PTS slave name");
    }
    unsafe {
        let slice = CStr::from_ptr(slavename);
        eprintln!("PTS slave: {:?}", slice);
    }


    let callsign = CString::new(callsign).expect("Failed to convert callsign to CString");
    let speed : i32 = 9600;
    let allow_broadcast : i32 = 1;

    let success = unsafe {
        kissattach(
            callsign.as_ptr(),
            speed,
            mtu,
            slavename,
            allow_broadcast)
    };

    if success == 0 {
        panic!("kissattach failed");
    }


    let mut radio = TcpStream::connect(fldigi_connect).unwrap();

    let mut pty_tx_side = master_file.try_clone()
        .expect("Cannot clone PTY file");

    let mut radio_tx = radio.try_clone().unwrap();

    let writer = thread::spawn(move || {
        loop {
            // read from pty, write to fldigi

            let mut buf = [0; 1024];
            let num_bytes_rx = pty_tx_side.read(&mut buf).unwrap();

            let txdata = &buf[0..num_bytes_rx];
            radio_tx.write(txdata).unwrap();
        }
    });

    loop {
        // read from fldigi, write to master_file

        let mut buf = [0; 1024];
        let num_bytes_rx = radio.read(&mut buf).unwrap();

        if num_bytes_rx == 0 {
            break;
        }

        let txdata = &buf[0..num_bytes_rx];
        master_file.write(txdata).unwrap();
    }

    writer.join().unwrap();
}
