
# TODO move the config and host file to a proper package
from pydantic import BaseModel, Field
from typing import Dict

class Host(BaseModel):
    name: str = Field(default="generic", description="Host name")
    core_count: int = Field(description="Number of CPU cores")
    core_per_numa: int = Field(description="Number of CPU cores per NUMA node")
    core_sequence:  str = Field(description="TODO fill this")
    
    def get_core_sequence_as_list(self):
        result = []
        for core_range in self.core_sequence.split(','):
            if '-' in core_range:
                start, end = map(int, core_range.split('-'))
                result += list(range(start, end + 1))
            else:
                result.append(int(core_range))
        return result


class SMTHost(Host):
    smt_sequence: str = Field(description="TODO fill this")



