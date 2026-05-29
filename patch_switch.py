from __future__ import annotations

import argparse
import json
import math
import sys
import wave
from pathlib import Path
from typing import Any

import numpy as np

import GammaDSP as dsp
from patch_dsl import PatchDSL
from patch_json import LoadedJSONPatch, load_patch_json
from swig_introspect import SWIGResolver


# ============================================================
# Output helpers
# ============================================================


def _json_default(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, (np.floating, np.integer)):
        return value.item()
    return str(value)


def emit(payload: dict[str, Any], as_json: bool) -> None:
    if as_json:
        print(json.dumps(payload, indent=2, sort_keys=False, default=_json_default))
        return

    ok = payload.get("ok", False)
    command = payload.get("command", "unknown")
    print(f"{command}: {'ok' if ok else 'failed'}")
    for key, value in payload.items():
        if key in {"ok", "command"}:
            continue
        print(f"{key}: {value}")


def fail(command: str, exc: BaseException, as_json: bool) -> int:
    payload = {
        "ok": False,
        "command": command,
        "error": str(exc),
        "error_type": exc.__class__.__name__,
    }
    emit(payload, as_json=as_json)
    return 1


# ============================================================
# Runtime helpers
# ============================================================


def _classes() -> list[str]:
    resolver = SWIGResolver()
    try:
        return list(resolver.list_classes())
    except AttributeError:
        return sorted(
            name
            for name in dir(dsp)
            if name and name[:1].isupper() and isinstance(getattr(dsp, name), type)
        )


def _audio_summary(audio: np.ndarray) -> dict[str, Any]:
    x = np.asarray(audio, dtype=np.float32)
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    rms = float(np.sqrt(np.mean(x * x))) if x.size else 0.0
    return {
        "frames": int(x.size),
        "dtype": str(x.dtype),
        "peak": peak,
        "rms": rms,
        "finite": bool(np.all(np.isfinite(x))),
        "first10": [float(v) for v in x[:10]],
    }


def _write_wav(path: str | Path, audio: np.ndarray, sample_rate: int) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    x = np.asarray(audio, dtype=np.float32)
    if x.ndim != 1:
        x = np.reshape(x, (-1,))
    if x.size:
        x = np.nan_to_num(x, nan=0.0, posinf=0.0, neginf=0.0)
        x = np.clip(x, -1.0, 1.0)
    pcm = (x * 32767.0).astype("<i2")

    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(int(sample_rate))
        wf.writeframes(pcm.tobytes())

    return path


def _read_input_json(path: str | Path) -> tuple[np.ndarray, int | None]:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    sample_rate: int | None = None

    if isinstance(data, dict):
        if "sample_rate" in data:
            sample_rate = int(data["sample_rate"])
        for key in ("samples", "audio", "input", "buffer"):
            if key in data:
                data = data[key]
                break
        else:
            raise ValueError(
                "Input JSON object must contain one of: samples, audio, input, buffer"
            )

    if not isinstance(data, list):
        raise ValueError("Input JSON must be a list of samples or an object containing samples")

    audio = np.asarray(data, dtype=np.float32)
    if audio.ndim != 1:
        audio = np.reshape(audio, (-1,))
    return audio, sample_rate


def _patch_summary(patch: LoadedJSONPatch, command: str, patch_path: str | Path) -> dict[str, Any]:
    node_names: list[str] = []
    try:
        node_names = list(patch.engine.nodes.keys())
    except Exception:
        node_names = []

    return {
        "ok": True,
        "command": command,
        "patch": str(patch_path),
        "format": "json",
        "sample_rate": int(getattr(patch.engine, "sample_rate", 0)),
        "block_size": int(getattr(patch.engine, "block_size", 0)),
        "duration": float(getattr(patch, "duration", 0.0)),
        "output": getattr(patch, "output_name", None),
        "node_count": len(node_names),
        "nodes": node_names,
        "has_automation": bool(patch.has_automation()),
    }


def _load_dsl(path: str | Path, strict: bool = False) -> PatchDSL:
    dsl = PatchDSL(strict=bool(strict))
    dsl.load_text(Path(path).read_text(encoding="utf-8"))
    return dsl


def _dsl_summary(dsl: PatchDSL, command: str, patch_path: str | Path) -> dict[str, Any]:
    node_names = list(getattr(dsl.engine, "nodes", {}).keys())
    render_specs = list(getattr(dsl, "render_specs", {}).keys())
    graph_defs = list(getattr(dsl, "graph_defs", {}).keys())
    chain_defs = list(getattr(dsl, "chain_defs", {}).keys())
    block_defs = list(getattr(dsl, "block_defs", {}).keys())
    return {
        "ok": True,
        "command": command,
        "patch": str(patch_path),
        "format": "dsl",
        "sample_rate": int(getattr(dsl, "sample_rate", 0)),
        "block_size": int(getattr(dsl, "block_size", 0)),
        "duration": float(getattr(dsl, "duration", 0.0)),
        "output": getattr(dsl, "output_name", None),
        "node_count": len(node_names),
        "nodes": node_names,
        "render_specs": render_specs,
        "graphs": graph_defs,
        "chains": chain_defs,
        "blocks": block_defs,
    }


# ============================================================
# Commands: status / reflection discovery
# ============================================================


def cmd_status(args: argparse.Namespace) -> dict[str, Any]:
    classes = _classes()
    return {
        "ok": True,
        "command": "status",
        "python": sys.version.split()[0],
        "python_executable": sys.executable,
        "gammadsp_file": getattr(dsp, "__file__", None),
        "class_count": len(classes),
        "has_runGenerator": hasattr(dsp, "runGenerator"),
        "has_runFunction": hasattr(dsp, "runFunction"),
        "has_runFunctionInPlace": hasattr(dsp, "runFunctionInPlace"),
        "has_FunctionGraph": hasattr(dsp, "FunctionGraph"),
        "has_DSPGraph": hasattr(dsp, "DSPGraph"),
    }


def cmd_classes(args: argparse.Namespace) -> dict[str, Any]:
    classes = _classes()
    if args.contains:
        needle = args.contains.lower()
        classes = [name for name in classes if needle in name.lower()]
    if args.limit is not None:
        classes = classes[: int(args.limit)]
    return {
        "ok": True,
        "command": "classes",
        "count": len(classes),
        "classes": classes,
    }


def cmd_describe(args: argparse.Namespace) -> dict[str, Any]:
    resolver = SWIGResolver()
    cls = resolver.resolve_class(args.class_name)
    method_names = sorted(
        name
        for name in dir(cls)
        if not name.startswith("_") and callable(getattr(cls, name, None))
    )
    return {
        "ok": True,
        "command": "describe",
        "class": args.class_name,
        "resolved_class": getattr(cls, "__name__", str(cls)),
        "method_count": len(method_names),
        "methods": method_names,
    }


# ============================================================
# Commands: JSON patch runner
# ============================================================


def cmd_validate(args: argparse.Namespace) -> dict[str, Any]:
    patch = load_patch_json(args.patch, strict=args.strict)
    return _patch_summary(patch, command="validate", patch_path=args.patch)


def cmd_inspect(args: argparse.Namespace) -> dict[str, Any]:
    patch = load_patch_json(args.patch, strict=args.strict)
    payload = _patch_summary(patch, command="inspect", patch_path=args.patch)
    if args.debug:
        payload["debug"] = patch.debug()
    if args.include_data:
        payload["data"] = patch.data
    return payload


def cmd_render(args: argparse.Namespace) -> dict[str, Any]:
    patch = load_patch_json(args.patch, strict=args.strict)
    audio = patch.render(duration=args.seconds)
    sample_rate = int(getattr(patch.engine, "sample_rate", 48000))
    summary = _audio_summary(audio)
    payload = {
        "ok": True,
        "command": "render",
        "patch": str(args.patch),
        "format": "json",
        "sample_rate": sample_rate,
        **summary,
    }
    if args.out:
        out_path = _write_wav(args.out, audio, sample_rate=sample_rate)
        payload["out"] = str(out_path)
    return payload


def cmd_process(args: argparse.Namespace) -> dict[str, Any]:
    if not args.input_json:
        raise ValueError("process currently requires --input-json")

    patch = load_patch_json(args.patch, strict=args.strict)
    input_audio, input_sample_rate = _read_input_json(args.input_json)
    audio = patch.process(input_audio)
    sample_rate = int(input_sample_rate or getattr(patch.engine, "sample_rate", 48000))
    summary = _audio_summary(audio)
    payload = {
        "ok": True,
        "command": "process",
        "patch": str(args.patch),
        "format": "json",
        "input_json": str(args.input_json),
        "input_frames": int(input_audio.size),
        "sample_rate": sample_rate,
        **summary,
    }
    if args.out:
        out_path = _write_wav(args.out, audio, sample_rate=sample_rate)
        payload["out"] = str(out_path)
    return payload


# ============================================================
# Commands: DSL patch runner
# ============================================================


def cmd_dsl_validate(args: argparse.Namespace) -> dict[str, Any]:
    dsl = _load_dsl(args.patch, strict=args.strict)
    return _dsl_summary(dsl, command="dsl-validate", patch_path=args.patch)


def cmd_dsl_inspect(args: argparse.Namespace) -> dict[str, Any]:
    dsl = _load_dsl(args.patch, strict=args.strict)
    payload = _dsl_summary(dsl, command="dsl-inspect", patch_path=args.patch)
    if args.debug:
        payload["debug"] = dsl.debug()
    return payload


def cmd_dsl_render(args: argparse.Namespace) -> dict[str, Any]:
    dsl = _load_dsl(args.patch, strict=args.strict)
    audio = dsl.render(target=args.target, duration=args.seconds)
    sample_rate = int(getattr(dsl, "sample_rate", 48000))
    summary = _audio_summary(audio)
    payload = {
        "ok": True,
        "command": "dsl-render",
        "patch": str(args.patch),
        "format": "dsl",
        "sample_rate": sample_rate,
        **summary,
    }
    if args.out:
        out_path = _write_wav(args.out, audio, sample_rate=sample_rate)
        payload["out"] = str(out_path)
    return payload


def cmd_dsl_process(args: argparse.Namespace) -> dict[str, Any]:
    if not args.input_json:
        raise ValueError("dsl-process currently requires --input-json")

    dsl = _load_dsl(args.patch, strict=args.strict)
    input_audio, input_sample_rate = _read_input_json(args.input_json)
    audio = dsl.process(input_audio, target=args.target)
    sample_rate = int(input_sample_rate or getattr(dsl, "sample_rate", 48000))
    summary = _audio_summary(audio)
    payload = {
        "ok": True,
        "command": "dsl-process",
        "patch": str(args.patch),
        "format": "dsl",
        "input_json": str(args.input_json),
        "input_frames": int(input_audio.size),
        "sample_rate": sample_rate,
        **summary,
    }
    if args.out:
        out_path = _write_wav(args.out, audio, sample_rate=sample_rate)
        payload["out"] = str(out_path)
    return payload


# ============================================================
# Parser / aliases
# ============================================================


def normalize_argv(argv: list[str]) -> list[str]:
    if not argv:
        return argv
    out = list(argv)
    if out and out[0] == "/switch":
        out = out[1:]

    aliases = {
        "patch.status": "status",
        "status-json": "status",
        "list-classes": "classes",
        "patch.classes": "classes",
        "classes-json": "classes",
        "describe-class": "describe",
        "patch.describe": "describe",
        "validate-json": "validate",
        "patch.validate": "validate",
        "inspect-json": "inspect",
        "patch.inspect": "inspect",
        "render-json": "render",
        "patch.render": "render",
        "process-json": "process",
        "patch.process": "process",
        "dsl.validate": "dsl-validate",
        "dsl.inspect": "dsl-inspect",
        "dsl.render": "dsl-render",
        "dsl.process": "dsl-process",
    }
    if out and out[0] in aliases:
        out[0] = aliases[out[0]]
    return out


def _add_json_flag(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--json", action="store_true")


def _add_strict_flag(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--strict", action="store_true")


def _add_output_flags(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--out", default=None, help="Optional output WAV path.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="patch_switch.py",
        description=(
            "Thin CLI runner for GammaDSP JSON/DSL patch documents. "
            "JSON and DSL remain the patch languages; this command only selects operations."
        ),
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("status", help="Print runtime/SWIG status.")
    _add_json_flag(p)
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("classes", help="List visible GammaDSP SWIG classes.")
    p.add_argument("--contains", default=None, help="Filter class names by substring.")
    p.add_argument("--limit", type=int, default=None, help="Limit result count.")
    _add_json_flag(p)
    p.set_defaults(func=cmd_classes)

    p = sub.add_parser("describe", help="Describe one GammaDSP class.")
    p.add_argument("class_name")
    _add_json_flag(p)
    p.set_defaults(func=cmd_describe)

    p = sub.add_parser("validate", help="Load a JSON patch and report its compiled shape.")
    p.add_argument("patch")
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_validate)

    p = sub.add_parser("inspect", help="Inspect a JSON patch without rendering audio.")
    p.add_argument("patch")
    p.add_argument("--debug", action="store_true", help="Include PatchEngine debug text.")
    p.add_argument("--include-data", action="store_true", help="Include raw patch JSON data.")
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_inspect)

    p = sub.add_parser("render", help="Render a JSON patch to a summary or WAV file.")
    p.add_argument("patch")
    p.add_argument("--seconds", type=float, default=None)
    _add_output_flags(p)
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_render)

    p = sub.add_parser("process", help="Process an input buffer through a JSON patch.")
    p.add_argument("patch")
    p.add_argument("--input-json", required=True, help="Input buffer JSON file.")
    _add_output_flags(p)
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_process)

    p = sub.add_parser("dsl-validate", help="Load a DSL patch and report its compiled shape.")
    p.add_argument("patch")
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_dsl_validate)

    p = sub.add_parser("dsl-inspect", help="Inspect a DSL patch without rendering audio.")
    p.add_argument("patch")
    p.add_argument("--debug", action="store_true", help="Include PatchEngine debug text.")
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_dsl_inspect)

    p = sub.add_parser("dsl-render", help="Render a DSL patch to a summary or WAV file.")
    p.add_argument("patch")
    p.add_argument("--target", default=None)
    p.add_argument("--seconds", type=float, default=None)
    _add_output_flags(p)
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_dsl_render)

    p = sub.add_parser("dsl-process", help="Process an input buffer through a DSL patch.")
    p.add_argument("patch")
    p.add_argument("--target", default=None)
    p.add_argument("--input-json", required=True, help="Input buffer JSON file.")
    _add_output_flags(p)
    _add_strict_flag(p)
    _add_json_flag(p)
    p.set_defaults(func=cmd_dsl_process)

    return parser


def main(argv: list[str] | None = None) -> int:
    argv = normalize_argv(list(sys.argv[1:] if argv is None else argv))
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        payload = args.func(args)
    except Exception as exc:
        return fail(args.command or "unknown", exc, as_json=getattr(args, "json", False))
    emit(payload, as_json=bool(getattr(args, "json", False)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
