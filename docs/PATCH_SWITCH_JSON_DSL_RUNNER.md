# Patch Switch JSON/DSL Runner

`patch_switch.py` is intentionally a thin runner. It is not a third patch language.

The PatchEngine stack remains:

```text
JSON patch documents / DSL patch documents
  -> patch_json.py / patch_dsl.py
  -> patch_engine.py
  -> swig_introspect.py
  -> GammaDSP.py / _GammaDSP*.so
```

## Design rule

The CLI selects an operation and file paths. JSON and DSL describe the patch.

Good CLI responsibilities:

- report runtime/SWIG status
- list/describe SWIG-visible classes for discovery
- validate JSON or DSL patch documents
- inspect JSON or DSL patch documents
- render generator-backed JSON or DSL patches
- process caller-supplied input buffers through JSON or DSL patches
- emit machine-readable JSON summaries

Avoid these CLI responsibilities:

- inventing a second or third patch language
- adding hardcoded DSP sources such as `process-sine` or `process-noise`
- duplicating PatchEngine construction semantics as many bespoke shell flags

## JSON commands

```bash
python patch_switch.py validate examples/hello_osc_filter_delay.json --json
python patch_switch.py inspect examples/hello_osc_filter_delay.json --debug --json
python patch_switch.py render examples/hello_osc_filter_delay.json --seconds 0.25 --out /tmp/render.wav --json
python patch_switch.py process examples/external_input_fx.json --input-json examples/input_ramp_short.json --out /tmp/fx.wav --json
```

## DSL commands

```bash
python patch_switch.py dsl-validate examples/hello_osc_filter_delay.dsl --json
python patch_switch.py dsl-inspect examples/hello_osc_filter_delay.dsl --debug --json
python patch_switch.py dsl-render examples/hello_osc_filter_delay.dsl --seconds 0.25 --out /tmp/dsl-render.wav --json
python patch_switch.py dsl-process examples/external_input_fx.dsl --input-json examples/input_ramp_short.json --out /tmp/dsl-fx.wav --json
```

## `/switch` aliases

```bash
python patch_switch.py /switch patch.inspect examples/hello_osc_filter_delay.json --json
python patch_switch.py /switch patch.process examples/external_input_fx.json --input-json examples/input_ramp_short.json --json
python patch_switch.py /switch dsl.inspect examples/hello_osc_filter_delay.dsl --json
python patch_switch.py /switch dsl.process examples/external_input_fx.dsl --input-json examples/input_ramp_short.json --json
```

## Input JSON format

`process` and `dsl-process` accept a caller-supplied JSON input buffer.

Valid forms:

```json
[0.0, 0.1, 0.2]
```

or:

```json
{
  "sample_rate": 48000,
  "samples": [0.0, 0.1, 0.2]
}
```

The CLI does not synthesize special source signals. If a test needs input, the input is an explicit data fixture.
