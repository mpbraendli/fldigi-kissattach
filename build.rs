// Copyright 2018 Matthias P. Braendli
// SPDX-License-Identifier: GPL-2.0-only

extern crate cc;

fn main() {
    cc::Build::new()
        .file("src/kissattach.c")
        .compile("kissattach");


    /*
       .file("bar.c")
       .define("FOO", Some("bar"))
       .include("src")
       .compile("foo");
     */
}
