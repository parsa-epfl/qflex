from importlib.machinery import SourceFileLoader
from pathlib import Path
from typer.main import get_command  

# Adjust if your script lives somewhere else:
SCRIPT_PATH = Path(__file__).resolve().parents[1] / "dep"

# Load the script as a module
_mod = SourceFileLoader("dep_script", str(SCRIPT_PATH)).load_module()

_typer_app = getattr(_mod, "app")
# Expose the Typer app for mkdocs-click
# If your Typer object is named something else (e.g. "cli"), change "app" below.
dep = get_command(_typer_app)
