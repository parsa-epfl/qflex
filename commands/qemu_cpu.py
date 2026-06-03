import os


DEFAULT_QEMU_CPU = "max,pauth=off,sme=off"
QEMU_CPU_ENV_VAR = "QFLEX_QEMU_CPU"


def resolve_qemu_cpu() -> str:
    return os.environ.get(QEMU_CPU_ENV_VAR, DEFAULT_QEMU_CPU)
