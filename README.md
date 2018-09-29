fldigi-kissattach
=================

Readme
------

fldigi-kissattach connects to the TCP KISS server of fldigi and patches that up
with the kernel in the same way kissattach connects a serial KISS TNC to the
kernel.

This allows you to run TCP/IP over AX.25 over any modem fldigi supports.

Compilation
-----------

This tool is written in Rust and needs a Rust compiler and the cargo package manager.
Please go to http://rustup.rs to install this.

Then execute `cargo build` inside the folder where this README resides. The output binary
will be written to `./target/debug/fldigi-kissattach`.

Usage
-----

fldigi-kissattach expects three arguments: callsign with SSID, ip:port to connect to fldigi, and mtu. You
will need to run this as root, best with sudo

Example: `sudo ./target/debug/fldigi-kissattach HB9EGM-1 127.0.0.1:7342 120`

I am unsure if having a corresponding entry in `/etc/ax25/axports` is required.

License
-------

Parts of this code are taken from the ax25-tools, which are GPL-2.0. Other parts are MIT or Apache 2.0 licensed.

New code is Copyright 2018 Matthias P. Braendli, GPL-2.0-only
