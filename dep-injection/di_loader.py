import importlib
import inspect
import types
from typing import Any, Union, get_type_hints, get_origin, get_args

from omegaconf import OmegaConf, DictConfig
from injector import Module, Provider, singleton


PRIMITIVE_TYPES = {int, str, float, bool, bytes, list, dict, tuple, set}
RESERVED_KEYS = {"_target_", "_scope_", "_name_", "_deps_"}
_OMIT = object()  # sentinel: skip kwarg, let constructor default apply


def import_callable(path: str):
    """Resolve 'pkg.module.Name' -> the class or function object."""
    if "." not in path:
        raise ValueError(f"_target_ must be fully qualified, got '{path}'")
    module_path, name = path.rsplit(".", 1)
    module = importlib.import_module(module_path)
    try:
        return getattr(module, name)
    except AttributeError as e:
        raise ImportError(f"'{name}' not found in '{module_path}'") from e


def _unwrap_optional(hint):
    origin = get_origin(hint)
    if origin is Union or origin is types.UnionType:
        args = [a for a in get_args(hint) if a is not type(None)]
        if len(args) == 1:
            return args[0], True
        return None, False
    return hint, False


def _unwrap_list(hint):
    origin = get_origin(hint)
    if origin is list:
        args = get_args(hint)
        if len(args) == 1 and inspect.isclass(args[0]):
            return args[0], True
        return None, False
    if inspect.isclass(hint):
        return hint, False
    return None, False


def _signature_for(target) -> tuple[inspect.Signature, dict]:
    """
    Return (signature, type_hints) for a class (its __init__) or function.
    """
    if inspect.isclass(target):
        return inspect.signature(target.__init__), get_type_hints(target.__init__)
    return inspect.signature(target), get_type_hints(target)


def find_typed_deps(target, scalar_param_names: set[str]):
    """
    Returns {param_name: (item_type, is_optional, is_list)} for non-primitive params
    that should be resolved through the injector graph.
    """
    sig, hints = _signature_for(target)

    typed_deps = {}
    for name in sig.parameters:
        if name == "self" or name in scalar_param_names:
            continue
        hint = hints.get(name)
        if hint is None:
            continue
        inner, is_optional = _unwrap_optional(hint)
        if inner is None:
            continue
        item_type, is_list = _unwrap_list(inner)
        if item_type is None or item_type in PRIMITIVE_TYPES:
            continue
        typed_deps[name] = (item_type, is_optional, is_list)
    return typed_deps


def get_return_type(target):
    """Class-targets return themselves; function-targets need a -> annotation."""
    if inspect.isclass(target):
        return target
    hints = get_type_hints(target)
    if "return" not in hints:
        raise ValueError(
            f"Factory '{target.__name__}' has no return type annotation; "
            f"add '-> SomeType' so injector knows what type it produces."
        )
    return hints["return"]


class ComponentProvider(Provider):
    def __init__(self, target, scalar_params, resolved_deps):
        self._target = target
        self._scalar_params = scalar_params
        self._resolved_deps = resolved_deps

    def get(self, injector):
        kwargs = dict(self._scalar_params)
        for param_name, dep in self._resolved_deps.items():
            if dep is _OMIT:
                continue
            if isinstance(dep, list):
                kwargs[param_name] = [injector.get(d) for d in dep]
            else:
                kwargs[param_name] = injector.get(dep)
        return self._target(**kwargs)


class ConfigDrivenModule(Module):
    def __init__(self, cfg: DictConfig):
        self._cfg = cfg

    def _make_binding_key(self, cls: type, name: str | None) -> type:
        if name is None:
            return cls
        cache_key = (cls, name)
        if cache_key not in self._key_cache:
            self._key_cache[cache_key] = type(f"{cls.__name__}__{name}", (cls,), {})
        return self._key_cache[cache_key]

    def configure(self, binder):
        self._key_cache: dict = {}
        components = self._cfg.get("components", {})

        # First pass: assign binding keys, detect duplicates
        component_keys: dict[str, type] = {}
        name_lookup: dict[tuple[type, str | None], type] = {}
        targets: dict[str, Any] = {}

        for comp_name, comp in components.items():
            target_path = comp.get("_target_")
            if target_path is None:
                raise ValueError(f"Component '{comp_name}' missing '_target_'")
            target = import_callable(target_path)
            return_type = get_return_type(target)
            name = comp.get("_name_")

            if (return_type, name) in name_lookup:
                raise ValueError(
                    f"Duplicate binding for {return_type.__name__}"
                    + (f" name='{name}'" if name else " (unnamed)")
                    + f" — second occurrence at component '{comp_name}'"
                )
            key = self._make_binding_key(return_type, name)
            component_keys[comp_name] = key
            name_lookup[(return_type, name)] = key
            targets[comp_name] = target

        # Second pass: build providers and bind
        for comp_name, comp in components.items():
            target = targets[comp_name]
            scope_name = comp.get("_scope_")

            comp_dict = OmegaConf.to_container(comp, resolve=True)
            scalar_params = {
                k: v for k, v in comp_dict.items() if k not in RESERVED_KEYS
            }
            explicit_deps = comp_dict.get("_deps_") or {}

            typed_deps = find_typed_deps(target, set(scalar_params.keys()))

            resolved_deps: dict[str, Any] = {}
            for param, (dep_type, is_optional, is_list) in typed_deps.items():
                if param in explicit_deps:
                    spec = explicit_deps[param]
                    if is_list:
                        resolved_deps[param] = [
                            name_lookup[(dep_type, e.get("name"))] for e in spec
                        ]
                    else:
                        resolved_deps[param] = name_lookup[(dep_type, spec.get("name"))]
                elif is_optional:
                    resolved_deps[param] = _OMIT
                elif is_list:
                    raise ValueError(
                        f"Required list dep '{param}' on '{comp_name}' "
                        f"needs a '_deps_' entry"
                    )
                else:
                    resolved_deps[param] = dep_type

            provider = ComponentProvider(target, scalar_params, resolved_deps)
            binding_key = component_keys[comp_name]

            if scope_name == "singleton":
                binder.bind(binding_key, to=provider, scope=singleton)
            elif scope_name is None:
                binder.bind(binding_key, to=provider)
            else:
                raise ValueError(
                    f"Unknown scope '{scope_name}' on component '{comp_name}'"
                )