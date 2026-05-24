from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "examples" / "hello_osc_filter_delay.json"


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


def test_status() -> None:
    payload = run_switch("status")
    assert payload["ok"] is True
    assert payload["has_runGenerator"] is True
    assert payload["has_runFunction"] is True
    assert payload["has_FunctionGraph"] or payload["has_DSPGraph"]


def test_classes() -> None:
    payload = run_switch("classes", "--contains", "Filter", "--limit", "20")
    assert payload["ok"] is True
    assert payload["count"] > 0
    assert any("Filter" in name for name in payload["classes"])


def test_describe() -> None:
    payload = run_switch("describe", "LowPassFilter")
    assert payload["ok"] is True
    assert payload["resolved_class"] == "LowPassFilter"
    assert "freq" in payload["methods"] or "setFreq" in payload["methods"]


def test_validate() -> None:
    payload = run_switch("validate", str(PATCH))
    assert payload["ok"] is True
    assert payload["node_count"] > 0
    assert payload["output"]


def test_render_summary_and_wav() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "render.wav"
        payload = run_switch(
            "render",
            str(PATCH),
            "--seconds",
            "0.05",
            "--out",
            str(out),
        )
        assert payload["ok"] is True
        assert payload["frames"] > 0
        assert payload["finite"] is True
        assert payload["peak"] > 1e-6
        assert out.exists()
        assert out.stat().st_size > 44


def main() -> None:
    test_status()
    test_classes()
    test_describe()
    test_validate()
    test_render_summary_and_wav()
    print("ALL PATCH SWITCH CLI TESTS PASSED")


if __name__ == "__main__":
    main()
