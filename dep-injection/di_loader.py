import importlib
import inspect
import types
from typing import Any, Union, get_type_hints, get_origin, get_args

from omegaconf import OmegaConf, DictConfig
from injector import Module, Provider, singleton


PRIMITIVE_TYPES = {int, str, float, bool, bytes, list, dict, tuple, set}
RESERVED_KEYS = {"_target_", "_scope_", "_name_", "_deps_"}

# Sentinel: skip this kwarg so the constructor's default applies.
_OMIT = object()


def import_class(path: str) -> type:
    """Resolve 'pkg.module.Class' -> the class object."""
    if "." not in path:
        raise ValueError(f"_target_ must be fully qualified, got '{path}'")
    module_path, class_name = path.rsplit(".", 1)
    module = importlib.import_module(module_path)
    try:
        return getattr(module, class_name)
    except AttributeError as e:
        raise ImportError(f"'{class_name}' not found in '{module_path}'") from e


def _unwrap_optional(hint):
    """Strip Optional[X] / Union[X, None] / X | None. Returns (inner_hint, is_optional)."""
    origin = get_origin(hint)
    if origin is Union or origin is types.UnionType:
        args = [a for a in get_args(hint) if a is not type(None)]
        if len(args) == 1:
            return args[0], True
        return None, False
    return hint, False


def _unwrap_list(hint):
    """Detect list[X]. Returns (item_type, is_list)."""
    origin = get_origin(hint)
    if origin is list:
        args = get_args(hint)
        if len(args) == 1 and inspect.isclass(args[0]):
            return args[0], True
        return None, False
    if inspect.isclass(hint):
        return hint, False
    return None, False


def find_typed_deps(cls: type, scalar_param_names: set[str]):
    """
    Walk __init__'s signature.
    Returns {param_name: (item_type, is_optional, is_list)} for non-primitive deps.
    """
    sig = inspect.signature(cls.__init__)
    hints = get_type_hints(cls.__init__)

    typed_deps: dict[str, tuple[type, bool, bool]] = {}
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


class ComponentProvider(Provider):
    """
    Build one component on demand:
    - scalar params come from YAML
    - typed deps are resolved through the injector graph
    - list deps are resolved per-element
    - _OMIT means "skip the kwarg, use the constructor default"
    """

    def __init__(self, cls, scalar_params, resolved_deps):
        self._cls = cls
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
        return self._cls(**kwargs)


class ConfigDrivenModule(Module):
    """Reads cfg.components and binds each entry to the injector."""

    def __init__(self, cfg: DictConfig):
        self._cfg = cfg

    def _make_binding_key(self, cls: type, name: str | None) -> type:
        """
        Unique type per (cls, name).
        Unnamed bindings use cls itself.
        Named bindings use a cached dynamic subclass so both passes agree.
        """
        if name is None:
            return cls
        cache_key = (cls, name)
        if cache_key not in self._key_cache:
            subclass = type(f"{cls.__name__}__{name}", (cls,), {})
            self._key_cache[cache_key] = subclass
        return self._key_cache[cache_key]

    def configure(self, binder):
        self._key_cache: dict[tuple[type, str | None], type] = {}
        components = self._cfg.get("components", {})

        # First pass: assign a binding key to every component, detect duplicates.
        component_keys: dict[str, type] = {}
        name_lookup: dict[tuple[type, str | None], type] = {}
        for comp_name, comp in components.items():
            target = comp.get("_target_")
            if target is None:
                raise ValueError(f"Component '{comp_name}' missing '_target_'")
            cls = import_class(target)
            name = comp.get("_name_")
            if (cls, name) in name_lookup:
                raise ValueError(
                    f"Duplicate binding for {cls.__name__}"
                    + (f" name='{name}'" if name else " (unnamed)")
                    + f" — second occurrence at component '{comp_name}'"
                )
            key = self._make_binding_key(cls, name)
            component_keys[comp_name] = key
            name_lookup[(cls, name)] = key

        # Second pass: build providers and bind them.
        for comp_name, comp in components.items():
            cls = import_class(comp["_target_"])
            scope_name = comp.get("_scope_")

            comp_dict = OmegaConf.to_container(comp, resolve=True)
            scalar_params = {
                k: v for k, v in comp_dict.items() if k not in RESERVED_KEYS
            }
            explicit_deps = comp_dict.get("_deps_") or {}

            typed_deps = find_typed_deps(cls, set(scalar_params.keys()))

            resolved_deps: dict[str, Any] = {}
            for param, (dep_type, is_optional, is_list) in typed_deps.items():
                if param in explicit_deps:
                    spec = explicit_deps[param]
                    if is_list:
                        resolved_deps[param] = [
                            name_lookup[(dep_type, entry.get("name"))]
                            for entry in spec
                        ]
                    else:
                        resolved_deps[param] = name_lookup[(dep_type, spec.get("name"))]
                elif is_optional:
                    resolved_deps[param] = _OMIT
                elif is_list:
                    raise ValueError(
                        f"Required list dep '{param}: list[{dep_type.__name__}]' on "
                        f"{cls.__name__} needs a '_deps_' entry"
                    )
                else:
                    resolved_deps[param] = dep_type

            provider = ComponentProvider(cls, scalar_params, resolved_deps)
            binding_key = component_keys[comp_name]

            if scope_name == "singleton":
                binder.bind(binding_key, to=provider, scope=singleton)
            elif scope_name is None:
                binder.bind(binding_key, to=provider)
            else:
                raise ValueError(
                    f"Unknown scope '{scope_name}' on component '{comp_name}'"
                )