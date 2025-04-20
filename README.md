# Bready Header-Only Libraries
Hi, I'm toast.
These are my header-only libraries.
They're all licensed under CC0:
feel free to do literally anything you want with them.

They are optimized for conceptual simplicity.
Most other libraries will be faster.
However, if you want to write your own for speed, these can also be a good
starting point

For now, they all target C99.
Though I may eventually target C11 or C23 for some of them,
when this happens the below list will mention the target C version.

They will not have any dependencies except libc.
Some of them may depend on POSIX rather than pure ISO C,
in which case the below list will note this.

## Library List
* arr.h (0.1): dynamic array based on the vlist data structure
* base64.h (0.2): implementation of "base64" and "base64url" compliant to RFC 4648
* ini.h (0.1): lax streaming parser for the INI format
* stdiox.h (0.1): extensions to ISO C stdio.h

## How do I use them?
1. Copy the appropriate `.h` file into your project.
2. `#include` the header.
3. In exactly one `.c` file, `#include` the header after defining the
   appropriate macro. This is usually something like
   `BREAD_uppercased-header-name_IMPLEMENTATION`.
   You can find the exact macro at the very start of the header.

For example, with base64. In your `.h`:
```c
#include "base64.h" // you may rename it
```
Then in exactly one `.c`:
```c
#define BREAD_BASE64_IMPLEMENTATION
#include "base64.h"
```

You can combine the implementations of all of the libraries in a single file,
they do not conflict with one another.

## Contributing
This repository follows the following contribution model:
* You can submit bugfixes for existing headers.
  When doing so, you should allow pushes from maintainers,
  your code *will* be changed.
* You can request additional features to existing headers.
  Depending on the complexity involved, those features may be implemented.
* You can request new headers.
  Do not expect those to be added any time soon.
* Benchmarks and Tests (in `/bench` and `/test`) are fair game.
  As long as they run on my machine, they will generally be merged.
  Make sure you add usage instructions at the top of the file.

Unless your code simplifies (*conceptually*, not computationally,
though computational simplification patches will be accepted if they do not
negatively affect conceptual simplicity) the existing code,
they will not be accepted.
However, if they do, they often will be.

Additionally, make sure to avoid defines whenever possible.
