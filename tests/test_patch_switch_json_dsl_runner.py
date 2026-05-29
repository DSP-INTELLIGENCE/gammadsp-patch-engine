from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JSON_RENDER_PATCH = ROOT / "examples" / "hello_osc_filter_delay.json"
JSON_PROCESS_PATCH = ROOT / "examples" / "external_input_fx.json"
DSL_RENDER_PATCH = ROOT / "examples" / "hello_osc_filter_delay.dsl"
DSL_PROCESS_PATCH = ROOT / "examples" / "external_input_fx.dsl"
INPUT_JSON = ROOT / "examples" / "input_ramp_short.json"


def run_switch(*args: str) -> dict:
    env = os.environ.copy()
    env["PYTHONPATH"] = f"{ROOT}{os.pathsep}{env.get('PYTHONPATH', '')}" if env.get("PYTHONPATH") else str(ROOT)
    proc = subprocess.run(
        [sys.executable, str(ROOT / "patch_switch.py"), *args, "--json"],
        cwd=str(ROOT),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert proc.returncode == 0, (
        f"command failed: {args}\n"
        f"stdout:\n{proc.stdout}\n"
        f"stderr:\n{proc.stderr}"
    )
    return json.loads(proc.stdout)


def test_json_inspect() -> None:
    payload = run_switch("inspect", str(JSON_RENDER_PATCH), "--debug")
    assert payload["ok"] is True
    assert payload["format"] == "json"
    assert payload["node_count"] > 0
    assert payload["output"]
    assert "debug" in payload


def test_json_process() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "json-process.wav"
        payload = run_switch(
            "process",
            str(JSON_PROCESS_PATCH),
            "--input-json",
            str(INPUT_JSON),
            "--out",
            str(out),
        )
    assert payload["ok"] is True
    assert payload["format"] == "json"
    assert payload["input_frames"] == 128
    assert payload["frames"] == 128
    assert payload["finite"] is True
    assert out.exists()
    assert out.stat().st_size > 44


def test_dsl_inspect() -> None:
    payload = run_switch("dsl-inspect", str(DSL_RENDER_PATCH), "--debug")
    assert payload["ok"] is True
    assert payload["format"] == "dsl"
    assert payload["node_count"] > 0
    assert payload["output"] == "main"
    assert "main" in payload["graphs"]
    assert "debug" in payload


def test_dsl_render() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "dsl-render.wav"
        payload = run_switch(
            "dsl-render",
            str(DSL_RENDER_PATCH),
            "--seconds",
            "0.05",
            "--out",
            str(out),
        )
    assert payload["ok"] is True
    assert payload["format"] == "dsl"
    assert payload["frames"] > 0
    assert payload["finite"] is True
    assert payload["peak"] > 1e-6
    assert out.exists()
    assert out.stat().st_size > 44


def test_dsl_process() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "dsl-process.wav"
        payload = run_switch(
            "dsl-process",
            str(DSL_PROCESS_PATCH),
            "--input-json",
            str(INPUT_JSON),
            "--out",
            str(out),
        )
    assert payload["ok"] is True
    assert payload["format"] == "dsl"
    assert payload["input_frames"] == 128
    assert payload["frames"] == 128
    assert payload["finite"] is True
    assert out.exists()
    assert out.stat().st_size > 44


def test_switch_aliases() -> None:
    payload = run_switch("/switch", "patch.inspect", str(JSON_RENDER_PATCH))
    assert payload["ok"] is True
    assert payload["command"] == "inspect"

    payload = run_switch("/switch", "dsl.inspect", str(DSL_RENDER_PATCH))
    assert payload["ok"] is True
    assert payload["command"] == "dsl-inspect"


def main() -> None:
    test_json_inspect()
    test_json_process()
    test_dsl_inspect()
    test_dsl_render()
    test_dsl_process()
    test_switch_aliases()
    print("ALL JSON/DSL RUNNER CLI TESTS PASSED")


if __name__ == "__main__":
    main()
