import os

import jinja2
import jinja2.meta


def render_interaction_script(experiment_context) -> str:
    """Render a .j2 interaction script into <exp>/scripts/ and return the rendered path.

    Non-.j2 paths (and empty) are returned untouched, so plain expect scripts keep working
    byte-identically. Variables come from ExperimentContext.interaction_template_vars();
    an empty-string value counts as not provided. Every variable the template uses but
    wasn't provided is reported in one error, with the YAML/CLI remedy.
    """
    path = experiment_context.interaction_script
    if not path or not path.endswith(".j2"):
        return path

    with open(path) as f:
        source = f.read()

    # Variables keep {{ }}. Block/comment delimiters are moved off Tcl-colliding sequences:
    # expect's prompt regex `-re {# +}` is Jinja's default comment-opener.
    env = jinja2.Environment(
        undefined=jinja2.StrictUndefined, keep_trailing_newline=True,
        block_start_string="<<%", block_end_string="%>>",
        comment_start_string="<<#", comment_end_string="#>>",
    )
    used = jinja2.meta.find_undeclared_variables(env.parse(source))

    all_vars = experiment_context.interaction_template_vars()
    provided = {k: v for k, v in all_vars.items() if v != ""}

    unsupported = sorted(used - all_vars.keys())
    if unsupported:
        raise ValueError(
            f"interaction script '{path}' uses unsupported template variable(s): "
            f"{', '.join('{{ %s }}' % v for v in unsupported)}. Supported variables: "
            f"{', '.join(sorted(all_vars))} (add new ones in ExperimentContext.interaction_template_vars)."
        )
    missing = sorted(used - provided.keys())
    if missing:
        raise ValueError(
            f"interaction script '{path}' uses {', '.join('{{ %s }}' % v for v in missing)} "
            f"but no value was provided — set {', '.join(repr(v) for v in missing)} on this leaf "
            f"in the YAML (or pass {', '.join('--' + v.replace('_', '-') for v in missing)}). "
            f"Supported variables: {', '.join(sorted(all_vars))}."
        )

    rendered = env.from_string(source).render(provided)
    out_path = (f"{experiment_context.get_experiment_folder_address()}/scripts/"
                f"{os.path.basename(path)[:-len('.j2')]}")
    # Regenerated on EVERY leaf run, atomically (tmp+rename): the output is always a complete
    # fresh render of the current template+vars — a reused experiment folder can't serve a
    # stale or half-written script.
    tmp_path = f"{out_path}.tmp.{os.getpid()}"
    with open(tmp_path, "w") as f:
        f.write(rendered)
    os.chmod(tmp_path, 0o755)
    os.replace(tmp_path, out_path)
    print(f"[interaction-script] rendered {path} -> {out_path} with {provided}")
    return out_path
