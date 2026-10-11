import importlib.util
from importlib.machinery import SourceFileLoader
from pathlib import Path
from typer.main import get_command  

# Adjust if your script lives somewhere else:
SCRIPT_PATH = Path(__file__).resolve().parents[1] / "dep"

# Load the script as a module
_loader = SourceFileLoader("dep_script", str(SCRIPT_PATH))
_spec = importlib.util.spec_from_loader(_loader.name, _loader)
assert _spec is not None
_mod = importlib.util.module_from_spec(_spec)
_loader.exec_module(_mod)

_typer_app = getattr(_mod, "app")
# Expose the Typer app for mkdocs-click
# If your Typer object is named something else (e.g. "cli"), change "app" below.
dep = get_command(_typer_app)
