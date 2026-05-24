# Essential file set

This is the first-copy candidate set for extracting the minimal GammaDSP PatchEngine runtime.

## SWIG/backend source

Copy first:

```text
GammaDSP.hpp
GammaDSP.cpp
GammaDSP.i
GammaDSP.sh
gamma_typemaps.i
Src/gamma_typemaps.i
include/GDSP/
```

Copy only if the build names them as missing:

```text
Gamma.i
GDSP.i
Analog.i
Array.i
CDSP.i
EchoMachine.i
TWavefolder.i
TWaveshaper.i
module.i
module.old.i
*.i files included by GammaDSP.i
```

Do not treat generated outputs as source of truth:

```text
_GammaDSP*.so
GammaDSP.py
GammaDSP_wrap.cxx
__pycache__/
```

Generated outputs may be release artifacts later, but the source repo should be buildable from source.

## PatchEngine runtime

Copy:

```text
patch_engine.py
patch_json.py
patch_dsl.py
swig_introspect.py
```

These are the current minimal runtime modules for live SWIG introspection and JSON/DSL patch loading.

## CLI switch layer

Create in this repo:

```text
patch_switch.py
```

It should call the current runtime stack:

```text
patch_switch.py -> patch_json.py -> patch_engine.py -> swig_introspect.py -> GammaDSP.py / _GammaDSP.so
```

## Tests

Copy current non-interactive tests into `tests/`:

```text
tests/test_current_swig_smoke.py
tests/test_introspection.py
tests/test_patch_json_introspection.py
```

Add later:

```text
tests/test_patch_switch_cli.py
```

Do not copy the full old `Tests/` folder into the minimal repo at first. Those files are human audio/graph demos.

## Examples

Create a small `examples/` folder:

```text
examples/hello_osc_filter_delay.json
examples/external_input_fx.json
examples/parallel_filter_graph.json
```

Start by copying/renaming a known-good JSON patch:

```text
gamma_patch_temp_hello.json -> examples/hello_osc_filter_delay.json
```

## Docs

Keep docs small and operational:

```text
README.md
docs/PATCH_ENGINE_BOUNDARY.md
docs/ESSENTIAL_FILES.md
docs/BUILD.md
docs/TESTS.md
docs/CLI_SWITCH_GOAL.md
docs/EXTRACTION_PLAN.md
docs/ROADMAP.md
```

## First excluded set

Do not copy unless a later branch proves the need:

```text
dynamic_factory.py
dynamic_registry.py
dynamic_patch_engine.py
dynamic_patch_dsl.py
factory.py
metadata.py
registry.py
registry_autogen.py
module_overlays.py
validate_registry.py
generate_metadata.py
Scripts/
Tests/
tools/
old root-level tests unrelated to current SWIG/JSON gates
```
