
from commands import Executor
from .config import ExperimentContext
from .jinja_loaders.parameterloader import ParameterLoader

class PartitionCommand(Executor):

    def __init__(self, 
                 partition_count: int,
                 warming_ratio: int, 
                 measurement_ratio: int):
        self.partition_count = partition_count
        self.detailed_warming_ratio = warming_ratio
        self.measurement_ratio = measurement_ratio
        # self.run_flexus_loader = ParameterLoader(
        #     experiment_context=ExperimentContext(),
        #     parameter_template='run_partition.sh.j2',
    
    def cmd(self) -> str:
        # TODO add run_flexus.sh to scripts folder
        # TODO get rid of partition at some point and move run_flexus.sh in our python commands
        return [
            f"./partition.py {self.partition_count}",
            f"./run_partition.sh {self.detailed_warming_ratio} {self.measurement_ratio}"
        ]