# Extraction plan

## Source repo

Use the known-good GammaDSP checkpoint:

```bash
cd ~/Downloads/GammaDSP
git checkout audit/json-introspection-01
git status --short
git log --oneline -3
git tag --list 'checkpoint-legacy-runner-json-introspection-pass'
```

## Target repo

Clone the new repo:

```bash
cd ~/Downloads
gh repo clone DSP-INTELLIGENCE/gammadsp-patch-engine
cd gammadsp-patch-engine
```

If the repo is empty, initialize docs first:

```bash
git status --short
```

## Copy docs package

Copy this package into the target repo root so it has:

```text
README.md
docs/
```

Commit docs first:

```bash
git add README.md docs
git commit -m "Add GammaDSP PatchEngine extraction docs"
git push origin main
```

## Copy first essential source set

From the target repo root:

```bash
cp ../GammaDSP/GammaDSP.hpp .
cp ../GammaDSP/GammaDSP.cpp .
cp ../GammaDSP/GammaDSP.i .
cp ../GammaDSP/GammaDSP.sh .
cp ../GammaDSP/gamma_typemaps.i .

mkdir -p Src include examples tests
cp ../GammaDSP/Src/gamma_typemaps.i Src/
cp -a ../GammaDSP/include/GDSP include/

cp ../GammaDSP/patch_engine.py .
cp ../GammaDSP/patch_json.py .
cp ../GammaDSP/patch_dsl.py .
cp ../GammaDSP/swig_introspect.py .

cp ../GammaDSP/test_current_swig_smoke.py tests/
cp ../GammaDSP/test_introspection.py tests/
cp ../GammaDSP/test_patch_json_introspection.py tests/

cp ../GammaDSP/gamma_patch_temp_hello.json examples/hello_osc_filter_delay.json
```

## Build and test

```bash
source ~/dsp/bin/activate
sh GammaDSP.sh

PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
```

## Missing-file policy

If SWIG or g++ reports a missing `.i` or header file, copy only that missing file from the source repo and rerun the same command.

Examples:

```bash
cp ../GammaDSP/GDSP.i .
cp ../GammaDSP/Analog.i .
cp ../GammaDSP/CDSP.i .
```

Do not bulk-copy the whole repository.

## First source commit

When the minimal build and tests pass:

```bash
git status --short
git add .
git commit -m "Extract minimal GammaDSP PatchEngine runtime"
git push origin main
```

## Next branch

```bash
git checkout -b feature/patch-switch-cli-01
```

Add `patch_switch.py` and `tests/test_patch_switch_cli.py` on that branch.
