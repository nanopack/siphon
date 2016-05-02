[![Build Status](https://travis-ci.org/nanopack/portal.svg)](https://travis-ci.org/nanopack/siphon)

# Siphon

Siphon is an output stream formatter, used primarily as a means to prefix output from a program.
Siphon can format data piped into it, or it can spawn a process and format that output.
When siphon formats the output of an exec'd command, it sets up a PTY and resizes the terminal to accommodate the prefix.

## Status
Complete/Stable

## Usage:

```
siphon - output stream formatter

Usage: siphon [options] [--  <command to exec>]
       siphon -p '-> ' or --prefix '-> '
       siphon -h or --help

Examples:
       ls -lah / | siphon --prefix '-> '
       siphon --prefix '-> ' -- ls -lah /
```

## Example

```
$ ls -lah | siphon --prefix '---> '
---> total 704
---> drwxr-xr-x  19 owner  group   646B Apr 26 13:41 .
---> drwxr-xr-x@ 58 owner  group   1.9K Apr 26 13:30 ..
---> -rw-r--r--   1 owner  group   133B Apr 26 13:31 .gitignore
---> -rw-r--r--   1 owner  group   1.1K Apr 26 17:28 LICENSE
---> -rw-r--r--   1 owner  group    23K Apr 26 13:41 Makefile
---> -rw-r--r--   1 owner  group    13B Apr 26 13:31 Makefile.am
---> -rw-r--r--   1 owner  group    23K Apr 26 13:41 Makefile.in
---> -rw-r--r--   1 owner  group   571B Apr 26 17:31 README.md
---> -rw-r--r--   1 owner  group    41K Apr 26 13:41 aclocal.m4
---> drwxr-xr-x   7 owner  group   238B Apr 26 13:41 autom4te.cache
---> -rwxr-xr-x   1 owner  group   7.2K Apr 26 13:41 compile
---> -rw-r--r--   1 owner  group   8.8K Apr 26 13:41 config.log
---> -rwxr-xr-x   1 owner  group    28K Apr 26 13:41 config.status
---> -rwxr-xr-x   1 owner  group   138K Apr 26 13:41 configure
---> -rw-r--r--   1 owner  group   408B Apr 26 13:35 configure.ac
---> -rwxr-xr-x   1 owner  group    23K Apr 26 13:41 depcomp
---> -rwxr-xr-x   1 owner  group    14K Apr 26 13:41 install-sh
---> -rwxr-xr-x   1 owner  group   6.7K Apr 26 13:41 missing
---> drwxr-xr-x   9 owner  group   306B Apr 26 17:30 src

```

### License

The MIT License (MIT)

[![open source](http://nano-assets.gopagoda.io/open-src/nanobox-open-src.png)](http://nanobox.io/open-source)
