[![Windows Build](https://github.com/roboter/antimony/actions/workflows/windows-build.yml/badge.svg)](https://github.com/roboter/antimony/actions/workflows/windows-build.yml)
[![Mac Build](https://github.com/roboter/antimony/actions/workflows/mac-build.yml/badge.svg)](https://github.com/roboter/antimony/actions/workflows/mac-build.yml)

## About
*Antimony* is a computer-aided design (CAD) tool from a parallel universe
in which CAD software evolved from Lisp machines rather than drafting tables

This work is a spiritual successor to [kokopelli](https://github.com/mkeeter/kokopelli)
by way of [fabserver](http://kokompe.cba.mit.edu).

For more details and screenshots, look at [this writeup](http://mattkeeter.com/projects/antimony).

https://github.com/roboter/antimony/blob/feature/win32-build/doc/screwdriver.mp4

## Try it
*Antimony* is in [long-term maintenance mode](https://github.com/mkeeter/antimony/issues/205#issuecomment-484271273).
It's at a beta level of stability:
solid, but not recommended for mission-critical use.

To get started, there are two suggested options:
- Download a [pre-built application](https://github.com/mkeeter/antimony/releases) (Mac and Windows)
- [Build from source](https://github.com/mkeeter/antimony/blob/develop/BUILDING.md) (Mac, Linux, and Windows)

There are also community-supported packages for the following operating systems:
* Debian 11 or later
    * `apt install antimony`
* Fedora 22 through 32 (no longer supported)
    * `dnf install antimony`
* FreeBSD 13 or later
    * `pkg install antimony-cad`
* Ubuntu 22.04 or later
    *  `apt install antimony`

## Mac Requirements

To build on macOS, you will need [Homebrew](https://brew.sh/) installed with the following packages:
- `qt@5`
- `python3`
- `boost-python3`
- `libpng`
- `cmake`
- `ninja`
- `flex`
- `lemon`

You can install these dependencies using:
```bash
brew install libpng python3 boost-python3 qt@5 lemon flex ninja cmake
```
See [BUILDING.md](BUILDING.md) for more details on compiling from source on Mac.

## Windows Requirements

To build on Windows, you will need [MSYS2](https://www.msys2.org/) with the UCRT64 environment and the following packages:
- `mingw-w64-ucrt-x86_64-toolchain`
- `mingw-w64-ucrt-x86_64-qt5`
- `mingw-w64-ucrt-x86_64-python`
- `mingw-w64-ucrt-x86_64-boost`
- `mingw-w64-ucrt-x86_64-libpng`
- `mingw-w64-ucrt-x86_64-cmake`
- `mingw-w64-ucrt-x86_64-ninja`
- `flex`
- `lemon`

You can use the provided PowerShell scripts (`.\scripts\build-windows.ps1` and `.\scripts\run-windows.ps1`) to automatically install these dependencies, build, and run the application. See [BUILDING.md](BUILDING.md) for more details.

## Support

If you have a general question, [send it along to the Google Group](https://groups.google.com/forum/#!forum/antimony-dev).

If you have a specific issue, [check the issues](https://github.com/mkeeter/antimony/issues) to see if someone else has had the same problem.

## License
Antimony is released under the MIT License:

Copyright (c) 2013-2015 Matthew Keeter and other contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Antimony includes code from [kokopelli](https://github.com/mkeeter/kokopelli), which is  
© 2012-2013 Massachusetts Institute of Technology  
© 2013 Matthew Keeter
