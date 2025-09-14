
import jinja2
from pathlib import Path
from .config import ExperimentContext, get_capital_dict

class ParameterLoader:

    def __init__(self,
                 experiment_context: ExperimentContext,
                 parameter_template: str,
                 output_name: str):
        self.experiment_context = experiment_context
        self.parameter_template = parameter_template
        self.output_name = output_name
    
    def load_parameters(self) -> str:
        jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader((Path(__file__).parent.parent / "cfg_templates").resolve()))
        template = jinja_env.get_template(self.parameter_template)
        context = get_capital_dict(self.experiment_context.simulation_context)
        parameter_file_path = f"./cfg/{self.experiment_context.experiment_name}/{self.output_name}"
        template.stream(context).dump(parameter_file_path)
        return parameter_file_path





