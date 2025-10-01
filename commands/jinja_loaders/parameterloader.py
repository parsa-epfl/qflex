
import jinja2
from pathlib import Path
from ..config import ExperimentContext, get_capital_dict
from abc import ABC, abstractmethod

class ParameterLoader(ABC):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 parameter_template: str,
                 output_name: str,
                 out_folder: str):
        self.experiment_context = experiment_context
        self.parameter_template = parameter_template
        self.output_name = output_name
        self.folder = out_folder
    
    def load_parameters(self) -> str:
        jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader("./templates"))
        template = jinja_env.get_template(self.parameter_template)
        context = self.get_context()
        parameter_file_path = f"{self.experiment_context.get_experiment_folder_address()}/{self.folder}/{self.output_name}"
        template.stream(context).dump(parameter_file_path)
        return parameter_file_path

    @abstractmethod
    def get_context(self) -> dict:
        # abstractmethod used to be get_capital_dict(self.experiment_context.simulation_context)
        raise NotImplementedError("Subclasses must implement get_context method")





