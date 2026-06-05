---
name: qflex-dependency-injection
description: Use when working on the YAML/DI configuration path of qflex — writing or debugging YAML configs in [conf/](../../../conf/), changing [dep_injection/](../../../dep_injection/) (builder, config_loader, di_loader), adding new components/factory functions, debugging injector errors, or wiring overrides. TRIGGER when the user mentions conf YAMLs, OmegaConf, python-injector, _target_/_scope_/_name_/_deps_, build_experiment_context, or "the DI graph". SKIP for changes that only touch CLI flags or factory body logic without touching the DI plumbing.
---

# qflex dependency injection

A YAML+DI layer on top of `create_experiment_context`. Loads an OmegaConf document, walks `components:` entries, builds an `injector.Injector` graph keyed by Pydantic return types, and resolves a single `ExperimentContext`.

## Entry points

The user-facing call is `build_experiment_context` in [dep_injection/builder.py](../../../dep_injection/builder.py):

```python
def build_experiment_context(
    config_path: str,
    cmd_name: str | None = None,                         # phase overlay key
    overrides: list[str] | None = None,                  # OmegaConf dotlist
    component_overrides: dict[str, dict] | None = None,  # structured
) -> ExperimentContext:
```

Pipeline: `load_config(path, cmd_name)` (which itself does `extends:` → phase overlay → `_base_:`) → `apply_cli_overrides(cfg, overrides)` → optional `OmegaConf.merge` of `component_overrides` → `Injector([ConfigDrivenModule(cfg)]).get(ExperimentContext)`.

The CLI calls this lazily from inside [`data_class_wrap`](../../../typer_inputs/config_wrapper.py); the wrapper passes `cmd_name = func.__name__` so the YAML's `<func_name>:` phase block (if any) is overlaid for this command. See the qflex-cli skill for that side.

## YAML shape

A config file is one OmegaConf document with three categories of recognized top-level keys:

- `extends: <bare-name>` — optional. Loads sibling `<name>.yaml` first, then deep-merges the current doc on top. Recursive (the parent's `extends:` is resolved first). **Bare name only** — `extends: dc` not `extends: dc.yaml` (the loader appends `.yaml` itself). Sibling lookup only — parent must live in the same directory.
- `components:` — a map of `<component-name>: { _target_: ..., ...params }`. The component-name is a free-form key used for diagnostics and `_deps_` references; the **actual injection key** is `(return_type, _name_)`.
- **Top-level base blocks** like `_leaf_defaults: { ... }` (or any other identifier; convention is leading underscore). Referenced by components via `_base_: <key>`. Resolved at the outermost `load_config` call so child YAMLs / phase overlays can mutate them before they get baked in.
- **Phase overlay blocks** `<func_name>: { ... }` — one per pipeline command (`boot`, `load`, `fw`, `run_idx`, …). Only applied when that command runs; the block mirrors the main YAML's shape (see "Phase overlays" below).

Real examples: [tests/realrun/base.yaml](../../../tests/realrun/base.yaml) (only `_leaf_defaults`, no components — used as a parent), [tests/realrun/single.yaml](../../../tests/realrun/single.yaml) (single-node, extends `base`), [tests/realrun/multi.yaml](../../../tests/realrun/multi.yaml) (multi-node fan-out via `_deps_.sub_experiments`), [tests/realrun/dc-multi.yaml](../../../tests/realrun/dc-multi.yaml) (drives every phase from one doc via phase overlays).

### Reserved keys inside a component

| Key | Purpose |
|---|---|
| `_target_` | **Required.** Fully-qualified callable, `pkg.module.Name`. A class returns itself; a factory function **must** have a `-> ReturnType` annotation (enforced by `get_return_type` in [di_loader.py:82](../../../dep_injection/di_loader.py#L82)). |
| `_scope_` | Optional. Only `singleton` is recognized. Anything else raises. |
| `_name_` | Optional. Named binding — lets two components produce the same return type. Implemented internally as a dynamic subclass `type(f"{cls.__name__}__{name}", (cls,), {})` so the injector treats them as distinct types. |
| `_deps_` | Optional. Explicit wiring for non-primitive params. `param_name: {name: '<other-component>'}` for scalar deps; `[{name: '...'}, {name: '...'}]` for `list[T]` deps. |

Everything else is passed straight to the target as a kwarg.

## Param resolution rules ([di_loader.py:113](../../../dep_injection/di_loader.py#L113))

`ConfigDrivenModule.configure` walks the components in two passes. For each component:

1. **Partition params.** Anything whose declared type is in `PRIMITIVE_TYPES` (`int, str, float, bool, bytes, list, dict, tuple, set`) is left as a scalar passed straight to the target.
2. **Find typed deps.** `find_typed_deps` introspects the target's signature + `get_type_hints`, unwrapping `Optional[T]` and `list[T]` (single-arg only). Anything that isn't primitive is a typed dep.
3. **Resolve each typed dep:**
   - In `_deps_`: look up `(dep_type, _deps_[param].name)` in `name_lookup`. List-typed deps use the same lookup per element.
   - Not in `_deps_` and `Optional[T]`: skip with `_OMIT` so the constructor's default applies.
   - Not in `_deps_` and **required `list[T]`**: error — required lists must be explicit.
   - Not in `_deps_` and required scalar `T`: bind to the unnamed `T` provider (assumes one component without `_name_` produces it).
4. Provider stores `(target, scalar_params, resolved_deps)`. At `.get()` time it pulls each typed dep from the injector and calls `target(**kwargs)`.

### Gotchas

- Two providers for the same return type **only** coexist when at least one has `_name_`. Duplicate `(return_type, _name_)` raises at module-configure time, not at `injector.get` — fail-fast.
- Factory functions must annotate `-> ReturnType`. Without it, `get_return_type` raises a clear message — don't bypass it by hand-binding.
- `_unwrap_list` only handles single-arg generics. `list[Union[A, B]]` won't resolve.

## Phase overlays (`<func_name>: { ... }`)

A phase overlay is a top-level block whose key matches the running Typer command's Python name (`boot`, `load`, `fw`, `run_idx`, `partition_cleanup`, …). The wrapper passes `cmd_name` to `load_config`, which pops `cfg[cmd_name]` and deep-merges it onto cfg **between** the `extends:` collapse and `_base_:` resolution. The block's shape mirrors the main YAML — only two targets are recognised:

- **`_leaf_defaults: { ... }`** — rule-level. Deep-merges into the top-level `_leaf_defaults` block. Propagates to every component using `_base_: _leaf_defaults` because base resolution runs after the phase merge. Use this when no component is hardcoding the field.
- **`components: { <name>: { ... } }`** — per-component. Deep-merges into the named component before `_base_:` resolves, so the override wins over the base AND over the component's own hardcoded fields. Use this when you need to scope strictly (e.g. only `experiment_context_node_1`, not the unnamed group), or when you must beat a per-leaf hardcoded value.

Flat-key forms — `boot: { latencies_ns_list: [...] }` — are **rejected** by [`_apply_phase_overlay`](../../../dep_injection/config_loader.py) so they don't silently no-op or land somewhere unexpected.

Worked example for [conf/WS/ws-multi.yaml](../../../conf/WS/ws-multi.yaml): boot needs a relaxed wire (high latency, sync off) but every other phase keeps the per-leaf default. Each leaf hardcodes `latencies_ns_list: [100000]`, so a phase `_leaf_defaults` change wouldn't beat it — use `components.<name>` instead:

```yaml
boot:
  components:
    experiment_context_node_0:
      latencies_ns_list: [1000000]
      syncs_list: ["false"]
    experiment_context_node_1:
      latencies_ns_list: [1000000]
      syncs_list: ["false"]
```

When a field isn't already hardcoded per-leaf, prefer `_leaf_defaults` for brevity:

```yaml
fw:
  _leaf_defaults:
    loadvm_name: init_warmed
```

## Override mechanisms (Python API)

In addition to the YAML-level layers above, `build_experiment_context` accepts two structured overrides applied after the YAML resolves:

1. **`overrides: list[str]`** — OmegaConf dotlists, applied via `apply_cli_overrides` before binding. Library-only; no CLI flag exposes it currently.
   ```python
   build_experiment_context("foo.yaml", overrides=["components.experiment_context.core_count=16",
                                                    "components.foo._name_=variant"])
   ```
2. **`component_overrides: dict[str, dict]`** — structured `{component_name: {field: value}}`. Merged via `OmegaConf.merge` *after* `apply_cli_overrides`. This is what `data_class_wrap` uses to splice per-flag CLI overrides on top of `-c <yaml>`.
   ```python
   build_experiment_context("foo.yaml", component_overrides={"experiment_context": {"core_count": 32}})
   ```

Both layer on after the phase overlay + `_base_:` resolution, and before any DI binding. Either can override `_target_`, `_scope_`, `_name_`, or any param.

## Final-value precedence (low → high; later wins)

Putting all the YAML/DI/CLI tiers together, this is the order in which any `ExperimentContext` field's value is resolved:

1. **Factory default** on `create_experiment_context`'s parameter.
2. **`extends:` chain** — recursive deep-merge with the child winning at every level (including `_leaf_defaults` and any other top-level base block). Resolved first, before anything else.
3. **Phase overlay** — top-level `<func_name>: { ... }` block, popped and deep-merged onto cfg by `_apply_phase_overlay` when the matching command runs. Two recognised forms inside the block: `_leaf_defaults: { ... }` (rule-level — flows through `_base_:` to every component) and `components: { <name>: { ... } }` (per-component — wins over `_base_` AND over per-leaf hardcoded fields). Flat-key forms are rejected.
4. **`_leaf_defaults` (via `_base_:`)** — each component with `_base_: _leaf_defaults` gets the (possibly phase-modified) base merged underneath via `merge(<base>, comp)` so component fields still win. Resolution happens once at the outermost `load_config` call, after the phase overlay.
5. **Per-component hardcoded values** in the component's own block (e.g. `experiment_context_node_0.loadvm_name`). Win over `_base_` because OmegaConf.merge keeps the component's keys on top.
6. **CLI flags** (`--memory-gb 8`, `--core-count 16`, …) — auto-generated by `data_class_wrap` per factory param. Wins over everything via the structured `component_overrides` path.

If a factory-required param has no value from any tier → `typer.BadParameter`. The check inside `data_class_wrap` reads the **fully resolved** group component (after extends + phase overlay + `_base_`) plus CLI keys, so the error is consistent across pure-CLI / pure-YAML / mixed and stays correct when the phase overlay introduces a field via `_leaf_defaults`.

## Adding a new component

1. Define a class or factory function. Factories must annotate `-> ReturnType` and should mark CLI-relevant params with `Annotated[T, Field(description="...")] = default` (Pydantic Field metadata) so the qflex-cli skill picks them up automatically.
2. Add a `components:` entry pointing at it via `_target_: pkg.module.Name`.
3. If it has typed (non-primitive) deps that aren't `Optional[T]`, add a `_deps_` block mapping each one to a sibling component's name.
4. If two providers should produce the same base type, give at least one a `_name_` and reference it via `_deps_`.

## Adding a field to `create_experiment_context`

If the new param is **primitive**: drop a kwarg into the YAML, no DI changes needed. If it's **non-primitive**: add a sibling component producing it plus (often) a `_deps_` entry.

In both cases, also add `Annotated[T, Field(description="...")] = default` to the factory parameter so the CLI auto-picks it up.

## Key files

- [dep_injection/builder.py](../../../dep_injection/builder.py) — `build_experiment_context`. Top-level entry; small.
- [dep_injection/config_loader.py](../../../dep_injection/config_loader.py) — `load_config` (extends → phase overlay → `_base_:`), `_apply_phase_overlay`, `_apply_component_bases`, `apply_cli_overrides`.
- [dep_injection/di_loader.py](../../../dep_injection/di_loader.py) — `ConfigDrivenModule`, `find_typed_deps`, `get_return_type`, `_unwrap_optional`/`_unwrap_list`. The real engine.
- [commands/config.py:467](../../../commands/config.py#L467) — `create_experiment_context`, the canonical factory.
- [conf/](../../../conf/) — example YAMLs.
- [tests/test_dep_injection_phase.py](../../../tests/test_dep_injection_phase.py) — unit tests that pin the phase-overlay precedence (rule-level / per-component / flat-key rejection / extends-chain merge).
