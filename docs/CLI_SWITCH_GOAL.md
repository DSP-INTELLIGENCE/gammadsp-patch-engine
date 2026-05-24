# CLI switch goal

## Goal

The PatchEngine must be completely controllable from CLI switches.

Python can remain the implementation language, and SWIG can remain the runtime bridge, but the public control surface should be command-line based so scripts, terminals, agents, and ML pipelines can use it without importing internal modules.

## Principle

```text
everything PatchEngine can do from Python
must also be possible from CLI switches
```

## First CLI file

Create:

```text
patch_switch.py
```

It should call the existing runtime stack:

```text
patch_switch.py
  -> patch_json.py
  -> patch_engine.py
  -> swig_introspect.py
  -> GammaDSP.py / _GammaDSP.so
```

Do not create a second engine.

## First commands

Initial command surface:

```bash
python patch_switch.py status
python patch_switch.py status --json
python patch_switch.py list-classes
python patch_switch.py describe-class LowPassFilter
python patch_switch.py validate-json examples/hello_osc_filter_delay.json
python patch_switch.py inspect-json examples/hello_osc_filter_delay.json
python patch_switch.py render-json examples/hello_osc_filter_delay.json --seconds 1.0 --out /tmp/gammadsp-test.wav
python patch_switch.py graph-json examples/hello_osc_filter_delay.json
```

## Slash switch aliases

After the plain CLI works, support slash-style operations:

```bash
python patch_switch.py /switch patch.status
python patch_switch.py /switch patch.classes
python patch_switch.py /switch patch.describe LowPassFilter
python patch_switch.py /switch patch.validate examples/hello_osc_filter_delay.json
python patch_switch.py /switch patch.render examples/hello_osc_filter_delay.json --seconds 1.0 --out /tmp/gammadsp-test.wav
```

## Machine-readable output

Every command that can be used by automation should support `--json`.

Example render result:

```json
{
  "ok": true,
  "command": "render-json",
  "input": "examples/hello_osc_filter_delay.json",
  "output": "/tmp/gammadsp-test.wav",
  "sample_rate": 48000,
  "frames": 48000,
  "seconds": 1.0
}
```

## Operation model

Use the switch-matrix operation shape:

```text
tool + action + target + options + permission + risk + persistence = operation
```

GammaDSP examples:

```text
patch_engine + status + runtime + json_output + read_only + safe + none
patch_engine + validate + json_patch + strict + read_only + safe + none
patch_engine + inspect + json_patch + summary + read_only + safe + none
patch_engine + render + json_patch + seconds/out + write_file + low + wav_output
patch_engine + list + swig_classes + filter + read_only + safe + none
```

## Safety boundaries

Initial CLI commands should be local-only and deterministic.

Avoid:

- background daemons
- network access
- hidden audio playback
- writing outside requested output paths
- modifying patch files unless an explicit save/update command is used

## First success condition

The first CLI branch is complete when these commands pass:

```bash
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python patch_switch.py status --json
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python patch_switch.py validate-json examples/hello_osc_filter_delay.json --json
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python patch_switch.py render-json examples/hello_osc_filter_delay.json --seconds 0.25 --out /tmp/gammadsp-test.wav --json
PYTHONPATH="$PWD${PYTHONPATH:+:$PYTHONPATH}" python tests/test_patch_switch_cli.py
```
