# swig_introspect.py
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any

import GammaDSP as dsp


def parse_value(text: str) -> Any:
    text = str(text).strip()

    if "," in text:
        return [parse_value(x) for x in text.split(",")]

    low = text.lower()

    if low in {"true", "yes", "on"}:
        return True
    if low in {"false", "no", "off"}:
        return False
    if low in {"none", "null"}:
        return None

    try:
        if "." in text or "e" in low:
            return float(text)
        return int(text)
    except ValueError:
        return text


def snake_to_camel(name: str) -> str:
    parts = name.split("_")
    return parts[0] + "".join(p[:1].upper() + p[1:] for p in parts[1:])


def setter_names(param: str) -> list[str]:
    camel = snake_to_camel(param)
    pascal = camel[:1].upper() + camel[1:]

    return [
        param,
        camel,
        f"set{pascal}",
        f"set_{param}",
    ]


def normalize_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


class SWIGResolver:
    def __init__(self, module=dsp):
        self.module = module
        self._class_map = self._build_class_map()

    def _build_class_map(self) -> dict[str, type]:
        out = {}

        for name in dir(self.module):
            if name.startswith("_"):
                continue

            obj = getattr(self.module, name)

            if isinstance(obj, type):
                out[name] = obj
                out[normalize_name(name)] = obj
                out[name.lower()] = obj

        return out

    def resolve_class(self, name: str) -> type:
        keys = [
            name,
            name.lower(),
            normalize_name(name),
        ]

        for key in keys:
            if key in self._class_map:
                return self._class_map[key]

        raise ValueError(f"Unknown GammaDSP class: {name}")

    def construct(self, class_name: str, ctor_args: list[Any] | None = None):
        ctor_args = ctor_args or []
        cls = self.resolve_class(class_name)
        return cls(*ctor_args)

    def resolve_method(self, obj: Any, param: str):
        # First try normal candidate names.
        for name in setter_names(param):
            if hasattr(obj, name):
                method = getattr(obj, name)
                if callable(method):
                    return method

        # Then try loose normalized lookup across dir(obj).
        target_names = {normalize_name(x) for x in setter_names(param)}

        for name in dir(obj):
            if name.startswith("_"):
                continue

            if normalize_name(name) in target_names:
                method = getattr(obj, name)
                if callable(method):
                    return method

        raise AttributeError(
            f"{obj.__class__.__name__} has no setter for parameter '{param}'"
        )

    def set_param(self, obj: Any, param: str, value: Any) -> None:
        method = self.resolve_method(obj, param)

        if isinstance(value, list):
            method(*value)
        else:
            method(value)

    def apply_params(self, obj: Any, params: dict[str, Any]) -> None:
        for key, value in params.items():
            self.set_param(obj, key, value)

    def infer_kind(self, obj: Any) -> str:
        try:
            if hasattr(self.module, "StereoFunction") and isinstance(obj, self.module.StereoFunction):
                return "stereo"
        except Exception:
            pass

        try:
            if hasattr(self.module, "Generator") and isinstance(obj, self.module.Generator):
                return "generator"
        except Exception:
            pass

        try:
            if hasattr(self.module, "Function") and isinstance(obj, self.module.Function):
                return "function"
        except Exception:
            pass

        name = obj.__class__.__name__.lower()

        if "graph" in name:
            return "generator"
        if "chain" in name or "block" in name:
            return "function"

        return "utility"

    def infer_category(self, obj: Any) -> str:
        name = obj.__class__.__name__.lower()

        if name == "blockdc":
            return "utility"
        if any(x in name for x in ["sine", "saw", "square", "osc", "buzz", "dsf", "dwo"]):
            return "oscillator"
        if any(x in name for x in ["filter", "biquad", "onepole", "reson", "notch", "shelf"]):
            return "filter"
        if any(x in name for x in ["delay", "comb", "chorus", "celeste"]):
            return "delay"
        if any(x in name for x in ["clip", "tanh"]):
            return "distortion"
        if any(x in name for x in ["graph", "chain", "block"]):
            return "routing"
        if any(x in name for x in ["env", "adsr", "ad", "seg", "curve"]):
            return "envelope"

        return "utility"

    def list_classes(self) -> list[str]:
        names = []

        for name in dir(self.module):
            if name.startswith("_"):
                continue

            obj = getattr(self.module, name)

            if isinstance(obj, type):
                names.append(name)

        return sorted(set(names))