# Firmware utilities

This repository is a collection of small, general purpose modules useful in
the development of firmware. The modules include:

- `debug.c`: Callback-based error handling
- `snprintf.c`: Portable `printf`-like functions
- `reg.c`: Register representation and handling

In addition, the following helpful scripts are included:

- `c2tex.py`: convert C source code into a LaTeX file
- `colorize.pl`: add colors to the output of Cppcheck

### Getting started

The code is written in standard C99 and has no external dependencies besides the
compiler, so it can be readily embedded in other projects. You can either
directly copy the files of interest, just the `utils/` directory, or include the
entire repository.

To generate PDF documentation and run tests:

    make doc # need python3 and pdflatex
    make tests # optional flags: ASAN=1 FANALYZER=1
    make check # need clang-format, intercept-build, clang-tidy, cppcheck, perl

### License

Copyright 2025 Stanford Research Systems, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
