import importlib.util
from importlib.machinery import SourceFileLoader
from pathlib import Path
from typer.main import get_command  

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "qflex"

_loader = SourceFileLoader("qflex_script", str(SCRIPT_PATH))
_spec = importlib.util.spec_from_loader(_loader.name, _loader)
assert _spec is not None
_mod = importlib.util.module_from_spec(_spec)
_loader.exec_module(_mod)

_typer_app = getattr(_mod, "app")
qflex = get_command(_typer_app)
