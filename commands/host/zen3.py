from .host import Host, SMTHost

Z3_NAME = 'zen3'
Z3_SMT_NAME = 'zen3_smt'
CORE_COUNT = 128
CORE_SEQUENCE = "0-127"
CORE_PER_NUMA = 64
SMT_SEQUENCE = '128-256'

Z3_HOST = Host(
    name=Z3_NAME,
    core_count=CORE_COUNT,
    core_per_numa=CORE_PER_NUMA,
    core_sequence=CORE_SEQUENCE
)
Z3_SMT_HOST = SMTHost(
    name=Z3_SMT_NAME,
    core_count=CORE_COUNT,
    core_per_numa=CORE_PER_NUMA,
    core_sequence=CORE_SEQUENCE,
    smt_sequence=SMT_SEQUENCE
)




