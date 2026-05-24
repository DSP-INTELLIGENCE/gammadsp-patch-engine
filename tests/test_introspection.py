# test_swig_introspection_patch.py
from __future__ import annotations

import math
import numpy as np

import GammaDSP as dsp

from swig_introspect import SWIGResolver
from patch_engine import PatchEngine


try:
    from patch_dsl import PatchDSL
    HAS_DSL = True
except Exception as exc:
    print("[WARN] Could not import dynamic_patch_dsl:", exc)
    HAS_DSL = False


SR = 48000
DURATION = 0.25
N = int(SR * DURATION)


def assert_audio_ok(name: str, audio: np.ndarray) -> None:
    assert isinstance(audio, np.ndarray), f"{name}: output is not ndarray"
    assert audio.dtype == np.float32, f"{name}: expected float32, got {audio.dtype}"
    assert audio.shape[0] > 0, f"{name}: empty audio"
    assert np.all(np.isfinite(audio)), f"{name}: non-finite samples"

    peak = float(np.max(np.abs(audio)))
    print(f"{name}: peak={peak:.6f}, first10={audio[:10]}")
    assert peak > 1e-6, f"{name}: silent output"


def test_resolver_classes_and_setters() -> None:
    print("\n=== test_resolver_classes_and_setters ===")

    resolver = SWIGResolver()

    saw_cls = resolver.resolve_class("Saw")
    saw_cls_alias = resolver.resolve_class("saw")

    assert saw_cls is saw_cls_alias
    assert saw_cls.__name__ == "Saw"

    osc = resolver.construct("Saw", [110, 0.0])
    assert resolver.infer_kind(osc) == "generator"

    lp = resolver.construct("LowPassFilter", [])
    assert resolver.infer_kind(lp) == "function"

    resolver.set_param(lp, "freq", 1200)
    resolver.set_param(lp, "res", 0.707)

    delay = resolver.construct("ModDelay", [2.0, 0.25])
    resolver.set_param(delay, "feedback", 0.2)
    resolver.set_param(delay, "mix", 0.25)

    print("Resolved classes and setters OK.")


def test_direct_swig_block_render() -> None:
    print("\n=== test_direct_swig_block_render ===")

    dsp.set_samplerate(SR)

    resolver = SWIGResolver()

    osc = resolver.construct("Saw", [110, 0.0])
    lp = resolver.construct("LowPassFilter")
    clip = resolver.construct("SoftClip")
    delay = resolver.construct("ModDelay", [2.0, 0.25])

    resolver.apply_params(lp, {"freq": 1200, "res": 0.707})
    resolver.apply_params(delay, {"feedback": 0.2, "mix": 0.25})

    x = np.zeros(N, dtype=np.float32)
    y1 = np.zeros_like(x)
    y2 = np.zeros_like(x)
    y3 = np.zeros_like(x)

    dsp.runGenerator(osc, x, N)
    dsp.runFunction(lp, x, y1, N)
    dsp.runFunction(clip, y1, y2, N)
    dsp.runFunction(delay, y2, y3, N)

    assert_audio_ok("direct_swig_block_render", y3)


def test_patch_engine_create_chain() -> None:
    print("\n=== test_patch_engine_create_chain ===")

    engine = PatchEngine(sample_rate=SR, block_size=128, strict=False)

    # Positional constructor args are passed through ctor_args.
    engine.create("osc", "Saw", ctor_args=[110, 0.0])
    engine.create("dc", "BlockDC")
    engine.create("lp", "LowPassFilter", freq=1200, res=0.707)
    engine.create("clip", "SoftClip")
    engine.create("delay", "ModDelay", ctor_args=[2.0, 0.25], feedback=0.2, mix=0.25)

    engine.make_chain("main_fx", ["dc", "lp", "clip", "delay"])

    audio = engine.render_generator_through("osc", "main_fx", DURATION)
    audio = engine.normalize(audio)

    print(engine.debug_summary())
    print(engine.describe_node("lp"))

    assert_audio_ok("patch_engine_create_chain", audio)


def test_patch_engine_graph_serial() -> None:
    print("\n=== test_patch_engine_graph_serial ===")

    engine = PatchEngine(sample_rate=SR, block_size=128, strict=False)

    engine.create("osc", "Saw", ctor_args=[110, 0.0])
    engine.create("dc", "BlockDC")
    engine.create("lp", "LowPassFilter", freq=1200, res=0.707)
    engine.create("clip", "SoftClip")
    engine.create("delay", "ModDelay", ctor_args=[2.0, 0.25], feedback=0.2, mix=0.25)

    engine.make_graph(
        "main",
        connections=[
            ("osc", "dc", 1.0),
            ("dc", "lp", 1.0),
            ("lp", "clip", 1.0),
            ("clip", "delay", 1.0),
        ],
        outputs=[
            ("delay", 1.0),
        ],
        normalize=False,
    )

    audio = engine.render_graph("main", DURATION)
    audio = engine.normalize(audio)

    print(engine.debug_summary())
    print(engine.describe_node("main"))

    assert_audio_ok("patch_engine_graph_serial", audio)


def test_patch_engine_graph_parallel() -> None:
    print("\n=== test_patch_engine_graph_parallel ===")

    engine = PatchEngine(sample_rate=SR, block_size=128, strict=False)

    engine.create("osc", "Saw", ctor_args=[90, 0.0])
    engine.create("lp", "LowPassFilter", freq=350, res=0.707)
    engine.create("bp", "BandPassFilter", freq=1200, res=6.0)
    engine.create("hp", "HighPassFilter", freq=3000, res=0.707)

    engine.make_graph(
        "main",
        connections=[
            ("osc", "lp", 1.0),
            ("osc", "bp", 1.0),
            ("osc", "hp", 1.0),
        ],
        outputs=[
            ("lp", 1.0),
            ("bp", 1.0),
            ("hp", 1.0),
        ],
        normalize=True,
    )

    audio = engine.render_graph("main", DURATION)
    audio = engine.normalize(audio)

    print(engine.debug_summary())
    print(engine.describe_node("main"))

    assert_audio_ok("patch_engine_graph_parallel", audio)


def test_patch_engine_external_input_graph() -> None:
    print("\n=== test_patch_engine_external_input_graph ===")

    engine = PatchEngine(sample_rate=SR, block_size=128, strict=False)

    engine.create("lp", "LowPassFilter", freq=800, res=0.707)
    engine.create("clip", "SoftClip")
    engine.create("delay", "ModDelay", ctor_args=[2.0, 0.25], feedback=0.2, mix=0.25)

    engine.make_graph(
        "fx",
        connections=[
            ("input", "lp", 1.0),
            ("lp", "clip", 1.0),
            ("clip", "delay", 1.0),
        ],
        outputs=[
            ("delay", 1.0),
        ],
    )

    t = np.arange(N, dtype=np.float32) / float(SR)
    x = (0.3 * np.sin(2.0 * math.pi * 110.0 * t)).astype(np.float32)

    audio = engine.process_graph("fx", x)
    audio = engine.normalize(audio)

    print(engine.debug_summary())
    print(engine.describe_node("fx"))

    assert_audio_ok("patch_engine_external_input_graph", audio)


def test_dsl_serial_graph() -> None:
    print("\n=== test_dsl_serial_graph ===")

    if not HAS_DSL:
        print("[SKIP] dynamic_patch_dsl not importable.")
        return

    patch = """
    sr 48000
    dur 0.25

    node osc Saw 110 0.0
    node dc BlockDC
    node lp LowPassFilter freq=1200 res=0.707
    node clip SoftClip
    node delay ModDelay 2.0 0.25 feedback=0.2 mix=0.25

    graph main osc -> dc -> lp -> clip -> delay
    out main
    """

    dsl = PatchDSL(strict=False)
    dsl.load_text(patch)

    audio = dsl.render()

    print(dsl.debug())
    assert_audio_ok("dsl_serial_graph", audio)


def test_dsl_parallel_graph() -> None:
    print("\n=== test_dsl_parallel_graph ===")

    if not HAS_DSL:
        print("[SKIP] dynamic_patch_dsl not importable.")
        return

    patch = """
    sr 48000
    dur 0.25

    node osc Saw 90 0.0

    node lp LowPassFilter freq=350 res=0.707
    node bp BandPassFilter freq=1200 res=6.0
    node hp HighPassFilter freq=3000 res=0.707

    graph main normalize=true
    connect osc -> lp
    connect osc -> bp
    connect osc -> hp

    graphout lp gain=1.0
    graphout bp gain=1.0
    graphout hp gain=1.0

    out main
    """

    dsl = PatchDSL(strict=False)
    dsl.load_text(patch)

    audio = dsl.render()

    print(dsl.debug())
    assert_audio_ok("dsl_parallel_graph", audio)


def main() -> None:
    test_resolver_classes_and_setters()
    test_direct_swig_block_render()
    test_patch_engine_create_chain()
    test_patch_engine_graph_serial()
    test_patch_engine_graph_parallel()
    test_patch_engine_external_input_graph()
    test_dsl_serial_graph()
    test_dsl_parallel_graph()

    print("\nALL SWIG INTROSPECTION PATCH TESTS PASSED")


if __name__ == "__main__":
    main()