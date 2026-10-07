# Third-Party Notices

Kunlun Desktop's original code is licensed under the root MIT license.

## Chromium Embedded Framework

CEF and Chromium are downloaded separately at the revision recorded in `cef/pin.json`; they are
not relicensed under MIT. The native preview copies the distribution's `LICENSE.txt` and
`CREDITS.html` into `Contents/Resources/licenses/`. Those notices must accompany redistribution.

The macOS application/helper startup and Views lifecycle in `src/cef/main_mac.mm`,
`src/cef/helper_mac.cc`, and `src/cef/app.cc`, and the macOS packaging patterns in
`src/cef/CMakeLists.txt`, are adapted from CEF's `cefsimple` examples:

Copyright (c) 2013–2014 The Chromium Embedded Framework Authors.
Portions copyright (c) 2010 The Chromium Authors. All rights reserved.

The CEF-derived portions are distributed under CEF's BSD license:

```text
Copyright (c) 2008-2020 Marshall A. Greenblatt. Portions Copyright (c)
2006-2009 Google Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

   * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
   * Neither the name of Google Inc. nor the name Chromium Embedded
Framework nor the names of its contributors may be used to endorse
or promote products derived from this software without specific prior
written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
