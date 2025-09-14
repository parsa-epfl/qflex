from enum import Enum
from typing import Dict

from .host import Host, SMTHost
from .saphire import SAPHIRE_HOST, SAPHIRE_SMT_HOST, SAPHIRE_NAME, SAPHIRE_SMT_NAME
from .zen3 import Z3_HOST, Z3_SMT_HOST, Z3_NAME, Z3_SMT_NAME

class HostType(str, Enum):
    ZEN3 = Z3_NAME
    ZEN3_SMT = Z3_SMT_NAME
    SAPHIRE = SAPHIRE_NAME
    SAPHIRE_SMT = SAPHIRE_SMT_NAME

HOSTS: Dict[HostType, Host | SMTHost] = {
    HostType.ZEN3: Z3_HOST,
    HostType.ZEN3_SMT: Z3_SMT_HOST,
    HostType.SAPHIRE: SAPHIRE_HOST,
    HostType.SAPHIRE_SMT: SAPHIRE_SMT_HOST,
}

__all__ = [
    "Host",
    "SMTHost",
    "HOSTS",
    "HostType",
]

