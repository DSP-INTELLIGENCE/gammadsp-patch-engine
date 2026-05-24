# Build guide

## Goal

Build the SWIG-generated GammaDSP Python backend from source so the PatchEngine runtime can import:

```python
import GammaDSP as dsp
import _GammaDSP
```

The build should produce something like:

```text
GammaDSP.py
_GammaDSP.cpython-310-x86_64-linux-gnu.so
GammaDSP_wrap.cxx
```

## Environment

Use the existing DSP Python environment if available:

```bash
source ~/dsp/bin/activate
```

The build requires:

- Python 3 development headers/config
- SWIG
- g++ with C++17 support
- NumPy for tests
- any Gamma/r8brain dependencies referenced by the source

## Build command

From repo root:

```bash
sh GammaDSP.sh
```

The current known-good include-path shape is:

```bash
swig -c++ -Iinclude -python GammaDSP.i

g++ -std=c++17 -fPIC -fmax-errors=1 -shared -O2 -D SWIG \
  -I. -I/usr/local/include/Gamma -Iinclude -I/usr/local/include/r8brain -Iinclude/GDSP/ \
  GammaDSP.cpp \
  GammaDSP_wrap.cxx \
  ...
```

The important rule is that `-Iinclude` must exist because many includes are written as:

```cpp
#include "GDSP/SomeHeader.hpp"
```

`-Iinclude/GDSP` alone is not enough for those include paths.

## Import verification

After building:

```bash
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python - <<'PY'
import GammaDSP as dsp
import _GammaDSP

print("GammaDSP file:", dsp.__file__)
print("_GammaDSP file:", _GammaDSP.__file__)
print("has runGenerator:", hasattr(dsp, "runGenerator"))
print("has runFunction:", hasattr(dsp, "runFunction"))
print("has FunctionGraph:", hasattr(dsp, "FunctionGraph"))
print("has DSPGraph:", hasattr(dsp, "DSPGraph"))
PY
```

Expected result:

```text
has runGenerator: True
has runFunction: True
has FunctionGraph: True
has DSPGraph: True
```

## Copy-on-failure rule

When extracting from the old repo, do not copy everything up front.

If SWIG or g++ fails with a missing file, copy only the named missing file or include directory, then rebuild. This keeps the new repo small and proves what is actually essential.

Example:

```text
GammaDSP.i:116: Error: Unable to find 'GDSP/Phase90.hpp'
```

Fix class:

- verify `include/GDSP/Phase90.hpp` exists
- ensure `GammaDSP.sh` includes `-Iinclude`
- copy that header only if it is actually absent

## Generated files policy

Generated files should normally remain untracked:

```text
GammaDSP.py
_GammaDSP*.so
GammaDSP_wrap.cxx
*.o
__pycache__/
```

A later release branch can package binaries separately.
