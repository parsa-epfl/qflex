from importlib.machinery import SourceFileLoader
from pathlib import Path
from typer.main import get_command  

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "qflex"

_mod = SourceFileLoader("qflex_script", str(SCRIPT_PATH)).load_module()

_typer_app = getattr(_mod, "app")
qflex = get_command(_typer_app)
