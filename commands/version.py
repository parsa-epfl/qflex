import os
def get_version() -> str:
    with open(os.path.join(os.path.dirname(__file__), '..', 'VERSION'), 'r') as f:
        version = f.read().strip()
    return version