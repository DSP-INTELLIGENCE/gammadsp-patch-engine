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
from patch_json import load_patch_json
from swig_introspect import SWIGResolver


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

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(int(sample_rate))
        wav.writeframes(pcm.tobytes())

    return path


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
        name for name in dir(cls)
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


def cmd_validate(args: argparse.Namespace) -> dict[str, Any]:
    patch = load_patch_json(args.patch, strict=args.strict)
    node_names = []
    try:
        node_names = list(patch.engine.nodes.keys())
    except Exception:
        node_names = []

    return {
        "ok": True,
        "command": "validate",
        "patch": str(args.patch),
        "sample_rate": int(getattr(patch.engine, "sample_rate", 0)),
        "block_size": int(getattr(patch.engine, "block_size", 0)),
        "duration": float(getattr(patch, "duration", 0.0)),
        "output": getattr(patch, "output_name", None),
        "node_count": len(node_names),
        "nodes": node_names,
    }


def cmd_render(args: argparse.Namespace) -> dict[str, Any]:
    patch = load_patch_json(args.patch, strict=args.strict)
    audio = patch.render(duration=args.seconds)
    sample_rate = int(getattr(patch.engine, "sample_rate", 48000))

    summary = _audio_summary(audio)
    payload = {
        "ok": True,
        "command": "render",
        "patch": str(args.patch),
        "sample_rate": sample_rate,
        **summary,
    }

    if args.out:
        out_path = _write_wav(args.out, audio, sample_rate=sample_rate)
        payload["out"] = str(out_path)

    return payload


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
        "render-json": "render",
        "patch.render": "render",
    }

    if out and out[0] in aliases:
        out[0] = aliases[out[0]]

    return out


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="patch_switch.py",
        description="CLI switch surface for the GammaDSP PatchEngine.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("status", help="Print runtime/SWIG status.")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("classes", help="List visible GammaDSP SWIG classes.")
    p.add_argument("--contains", default=None, help="Filter class names by substring.")
    p.add_argument("--limit", type=int, default=None, help="Limit result count.")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_classes)

    p = sub.add_parser("describe", help="Describe one GammaDSP class.")
    p.add_argument("class_name")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_describe)

    p = sub.add_parser("validate", help="Load a JSON patch and report its compiled shape.")
    p.add_argument("patch")
    p.add_argument("--strict", action="store_true")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_validate)

    p = sub.add_parser("render", help="Render a JSON patch to a summary or WAV file.")
    p.add_argument("patch")
    p.add_argument("--seconds", type=float, default=None)
    p.add_argument("--out", default=None)
    p.add_argument("--strict", action="store_true")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=cmd_render)

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
