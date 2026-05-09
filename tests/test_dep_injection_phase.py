"""Tests for the phase-overlay (`<cmd_name>:` top-level block) in
dep_injection/config_loader.py — covers `_leaf_defaults` rule-level changes,
`components.<name>` per-component scoping, and the rejection of flat-key forms."""
import textwrap

import pytest

from dep_injection.config_loader import load_config


def _write(path, body):
    path.write_text(textwrap.dedent(body))


def test_phase_overlay_absent_no_op(tmp_path):
    """No cmd_name → phase blocks stay dormant; the loaded shape is unchanged."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          a: 1
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
        boot:
          _leaf_defaults:
            a: 99
    """)
    cfg = load_config(tmp_path / "c.yaml")
    assert cfg.components.x.a == 1
    assert cfg.boot._leaf_defaults.a == 99   # phase block stays in cfg, dormant


def test_phase_overlay_leaf_defaults_propagates(tmp_path):
    """`<cmd>: { _leaf_defaults: {...} }` flows into every component using
    `_base_: _leaf_defaults`."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          latency: 100
          sync: true
        components:
          group:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
          leaf:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
        boot:
          _leaf_defaults:
            latency: 1000
    """)
    cfg = load_config(tmp_path / "c.yaml", cmd_name="boot")
    assert cfg.components.group.latency == 1000
    assert cfg.components.leaf.latency == 1000
    assert cfg.components.group.sync is True   # untouched fields stick
    assert "boot" not in cfg                   # phase block popped


def test_phase_overlay_per_component_scope(tmp_path):
    """`<cmd>: { components.<name>: {...} }` scopes the override to that
    component only — other components and the group are untouched."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          latency: 100
        components:
          group:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
          leaf_a:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
          leaf_b:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
        boot:
          components:
            leaf_a:
              latency: 1000
    """)
    cfg = load_config(tmp_path / "c.yaml", cmd_name="boot")
    assert cfg.components.group.latency == 100
    assert cfg.components.leaf_a.latency == 1000   # explicitly scoped
    assert cfg.components.leaf_b.latency == 100


def test_phase_overlay_per_component_beats_hardcoded(tmp_path):
    """A `components.<name>` override wins over the component's per-leaf
    hardcoded value — the whole point, since `_leaf_defaults` would lose."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          latency: 100
        components:
          leaf:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
            latency: 50    # hardcoded, beats _leaf_defaults
        boot:
          _leaf_defaults:
            latency: 1000
          components:
            leaf:
              latency: 9999
    """)
    cfg = load_config(tmp_path / "c.yaml", cmd_name="boot")
    # _leaf_defaults overlay would lose to the hardcoded `latency: 50`;
    # the components.leaf overlay wins because it's merged before _base_.
    assert cfg.components.leaf.latency == 9999


def test_phase_overlay_flat_key_rejected(tmp_path):
    """Flat keys at the top of a phase block are rejected to force explicit
    scoping (`_leaf_defaults: {...}` or `components: {...}`)."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          a: 1
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
        boot:
          a: 99
    """)
    with pytest.raises(ValueError, match="unrecognised top-level key"):
        load_config(tmp_path / "c.yaml", cmd_name="boot")


def test_phase_overlay_unknown_command_no_op(tmp_path):
    """If cmd_name doesn't match any top-level block, nothing happens."""
    _write(tmp_path / "c.yaml", """
        _leaf_defaults:
          a: 1
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
    """)
    cfg = load_config(tmp_path / "c.yaml", cmd_name="missing_phase")
    assert cfg.components.x.a == 1


def test_phase_overlay_extends_chain_winner(tmp_path):
    """A child YAML's `<cmd>:` block overrides a parent YAML's same-named
    block at every level — standard deep-merge with child winning."""
    _write(tmp_path / "parent.yaml", """
        _leaf_defaults:
          a: 1
          b: 2
        components:
          x:
            _target_: pkg.mod.Cls
            _base_: _leaf_defaults
        boot:
          _leaf_defaults:
            a: 100
            b: 200
    """)
    _write(tmp_path / "child.yaml", """
        extends: parent
        boot:
          _leaf_defaults:
            a: 9999    # child wins; b: 200 inherited
    """)
    cfg = load_config(tmp_path / "child.yaml", cmd_name="boot")
    assert cfg.components.x.a == 9999
    assert cfg.components.x.b == 200
