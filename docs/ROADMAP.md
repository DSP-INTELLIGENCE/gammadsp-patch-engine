# Roadmap

This roadmap is the planning source for humans, ChatGPT, and future Codex runs. Checked items are already completed or represented by the current repository state. Unchecked items are the next planned branches.

## Phase 0: extraction baseline

- [x] Create `gammadsp-patch-engine` as a smaller extracted repo.
- [x] Add initial README and extraction docs.
- [x] Copy the minimal PatchEngine runtime source.
- [x] Build `_GammaDSP` from source with active Python and NumPy headers.
- [x] Remove generated/precompiled header artifacts from git.
- [x] Add `automation.py` as a required JSON PatchEngine dependency.
- [x] Ignore local patch/test output directories.
- [x] Tag `checkpoint-minimal-patch-engine-json-pass`.

## Phase 1: current runtime gates

- [x] `tests/test_current_swig_smoke.py` passes.
- [x] `tests/test_introspection.py` passes.
- [x] `tests/test_patch_json_introspection.py` passes.
- [x] SWIG classes resolve reflectively through `swig_introspect.py`.
- [x] JSON patch load/render/process paths work.
- [x] FunctionGraph and external-input graph paths work.

## Phase 2: CLI runner baseline

- [x] Add `patch_switch.py` with status/classes/describe/validate/render.
- [x] Add `tests/test_patch_switch_cli.py`.
- [x] Keep CLI as runner, not a third patch language.
- [x] Update example patch to use a generator-backed render path.
- [x] Tag `checkpoint-patch-switch-cli-render-01`.

## Phase 3: JSON/DSL runner baseline

- [x] Refine `patch_switch.py` around JSON/DSL patch documents.
- [x] Add JSON and DSL examples for generator and processor patch modes.
- [x] Add `process` support with caller-supplied input JSON buffers.
- [x] Fix processor examples to use only supported SWIG setters.
- [x] Fix tests to check temp output files before temp dirs are deleted.
- [x] Tag `checkpoint-json-dsl-runner-process-01`.

## Phase 4: patch design language

- [ ] Add a formal patch authoring guide.
- [ ] Define patch metadata conventions: `version`, `name`, `description`, `author`, `tags`.
- [ ] Define I/O contracts: `io.mode = generator|processor|analysis|utility`.
- [ ] Define controls as a public patch interface separate from raw node params.
- [ ] Define automation conventions for node params and future control-level automation.
- [ ] Add schema-lite validation tests for examples.
- [ ] Add more canonical patch examples: generator, processor, parallel graph, automation sweep.

Suggested branch:

```text
feature/patch-design-guide-01
```

## Phase 5: patch library and datasets

- [ ] Create a curated `examples/library/` or `patches/library/` folder.
- [ ] Add deterministic render/process batch commands if needed.
- [ ] Add JSONL manifest generation for ML/data workflows.
- [ ] Add dataset-safe outputs with explicit paths and no playback/GUI assumptions.
- [ ] Add regression signatures for example audio summaries.

## Phase 6: Codex-ready operations

- [x] Add staged patch ZIP workflow.
- [ ] Add `AGENTS.md` with contributor and agent rules.
- [ ] Add ChatGPT handoff prompts.
- [ ] Add Codex handoff prompts and branch templates.
- [ ] Add CI or a local `scripts/run_all_gates.sh` wrapper.
- [ ] Add issue/PR templates if the repo starts using GitHub Issues.

## Phase 7: optional UI/demo layer

- [ ] Keep old human audio/graph demos separate from tests.
- [ ] Add demo docs only after the machine-checkable layer is stable.
- [ ] Avoid audio-device or graph-window requirements in tests.
