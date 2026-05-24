# Patch Switch CLI

`patch_switch.py` is the first CLI `/switch` surface over the minimal GammaDSP PatchEngine repo.

It does not introduce a second engine. It calls the current runtime stack:

```text
patch_switch.py
  -> patch_json.py
  -> patch_engine.py
  -> swig_introspect.py
  -> GammaDSP.py / _GammaDSP*.so
```

## Commands

```bash
python patch_switch.py status --json
python patch_switch.py classes --json
python patch_switch.py classes --contains Filter --limit 20 --json
python patch_switch.py describe LowPassFilter --json
python patch_switch.py validate examples/hello_osc_filter_delay.json --json
python patch_switch.py render examples/hello_osc_filter_delay.json --seconds 0.25 --out /tmp/gammadsp.wav --json
```

## `/switch` aliases

The CLI also accepts early switch-matrix aliases:

```bash
python patch_switch.py /switch patch.status --json
python patch_switch.py /switch patch.classes --json
python patch_switch.py /switch patch.describe LowPassFilter --json
python patch_switch.py /switch patch.validate examples/hello_osc_filter_delay.json --json
python patch_switch.py /switch patch.render examples/hello_osc_filter_delay.json --seconds 0.25 --json
```

Compatibility aliases are also accepted:

```bash
python patch_switch.py list-classes --json
python patch_switch.py describe-class LowPassFilter --json
python patch_switch.py validate-json examples/hello_osc_filter_delay.json --json
python patch_switch.py render-json examples/hello_osc_filter_delay.json --seconds 0.25 --json
```

## Rules

- JSON output is the machine interface.
- JSON patches remain the portable patch state.
- `ctor_args` remain positional C++ constructor arguments.
- `params` remain dynamic setter calls.
- Graph rendering still goes through the existing PatchEngine/FunctionGraph path.
- WAV writing is a CLI convenience; generated audio files are ignored by git.

## Tests

```bash
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_current_swig_smoke.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_json_introspection.py
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_cli.py
```
