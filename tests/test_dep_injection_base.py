"""Tests for the `_base_` reserved key in dep_injection/config_loader.py."""
import textwrap

import pytest

from dep_injection.config_loader import load_config


def _write(path, body):
    path.write_text(textwrap.dedent(body))


def test_base_merges_top_level_dict(tmp_path):
    """Component fields override _base_ fields; _base_ key is stripped."""
    _write(tmp_path / "c.yaml", """
        _shared:
          a: 1
          b: 2
          c: 3
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _shared
            b: 99
    """)
    cfg = load_config(tmp_path / "c.yaml")
    x = cfg.components.x
    assert "_base_" not in x
    assert x.a == 1            # from base
    assert x.b == 99           # component overrides base
    assert x.c == 3            # from base
    assert x._target_ == "pkg.mod.Cls"


def test_base_resolves_against_extends_chain(tmp_path):
    """A child's `_base_` can target a top-level key its parent provides."""
    _write(tmp_path / "parent.yaml", """
        _shared:
          a: 1
          b: 2
    """)
    _write(tmp_path / "child.yaml", """
        extends: parent
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _shared
            b: 99
    """)
    cfg = load_config(tmp_path / "child.yaml")
    x = cfg.components.x
    assert "_base_" not in x
    assert x.a == 1
    assert x.b == 99


def test_base_missing_top_level_raises(tmp_path):
    _write(tmp_path / "c.yaml", """
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _nope
    """)
    with pytest.raises(ValueError, match="_base_: '_nope'"):
        load_config(tmp_path / "c.yaml")


def test_base_pointing_at_scalar_raises(tmp_path):
    _write(tmp_path / "c.yaml", """
        _shared: 42
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _shared
    """)
    with pytest.raises(ValueError, match="must point at a mapping"):
        load_config(tmp_path / "c.yaml")
