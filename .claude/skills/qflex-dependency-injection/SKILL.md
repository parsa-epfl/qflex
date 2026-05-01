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
    overrides: list[str] | None = None,                  # OmegaConf dotlist
    component_overrides: dict[str, dict] | None = None,  # structured
) -> ExperimentContext:
```

Pipeline: `load_config(path)` → `apply_cli_overrides(cfg, overrides)` → optional `OmegaConf.merge` of `component_overrides` → `Injector([ConfigDrivenModule(cfg)]).get(ExperimentContext)`.

The CLI calls this lazily from inside [`data_class_wrap`](../../../typer_inputs/config_wrapper.py); see the qflex-cli skill for that side.

## YAML shape

A config file is one OmegaConf document with two recognized top-level keys:

- `extends: <bare-name>` — optional. Loads sibling `<name>.yaml` first, then merges current doc on top. Recursive. **Bare name only** — `extends: dc` not `extends: dc.yaml` (the loader appends `.yaml` itself; passing `dc.yaml` makes it look up `dc.yaml.yaml`). Sibling lookup only — parent must live in the same directory.
- `components:` — a map of `<component-name>: { _target_: ..., ...params }`. The component-name is a free-form key used for diagnostics and `_deps_` references; the **actual injection key** is `(return_type, _name_)`.

Real examples: [conf/dc.yaml](../../../conf/dc.yaml) (single-node base) and [conf/dc-node-0.yaml](../../../conf/dc-node-0.yaml) (multi-node, extends `dc`).

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

## Override mechanisms

Two ways to override values on top of a loaded YAML:

1. **`overrides: list[str]`** — OmegaConf dotlists, applied via `apply_cli_overrides` before binding. Library-only; no CLI flag exposes it currently.
   ```python
   build_experiment_context("foo.yaml", overrides=["components.experiment_context.core_count=16",
                                                    "components.foo._name_=variant"])
   ```
2. **`component_overrides: dict[str, dict]`** — structured `{component_name: {field: value}}`. Merged via `OmegaConf.merge` *after* `apply_cli_overrides`. This is what `data_class_wrap` uses to splice per-flag CLI overrides on top of `-c <yaml>`.
   ```python
   build_experiment_context("foo.yaml", component_overrides={"experiment_context": {"core_count": 32}})
   ```

Both layer on after `extends:` resolution and before any binding. Either can override `_target_`, `_scope_`, `_name_`, or any param.

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
- [dep_injection/config_loader.py](../../../dep_injection/config_loader.py) — `load_config` (recursive `extends:`) and `apply_cli_overrides`.
- [dep_injection/di_loader.py](../../../dep_injection/di_loader.py) — `ConfigDrivenModule`, `find_typed_deps`, `get_return_type`, `_unwrap_optional`/`_unwrap_list`. The real engine.
- [commands/config.py:467](../../../commands/config.py#L467) — `create_experiment_context`, the canonical factory.
- [conf/](../../../conf/) — example YAMLs.
