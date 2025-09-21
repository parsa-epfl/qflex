class TyperDataClassMeta:
    """
    A class to hold metadata for Typer CLI dataclass inputs.
    """
    def __init__(self, name: str, init_function: callable):
        self.name = name
        self.init_function = init_function