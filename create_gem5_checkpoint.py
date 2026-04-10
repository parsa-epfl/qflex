"""
Generate a gem5-compatible m5.cpt checkpoint from the artefacts produced by
savevm.c when ``generate_gem5_chkpt`` is enabled.

Expected input directory (``<snapshot>.gem/``):
    register-info.json           CPU register state (JSON, hex strings)
    dev.info                     VirtIO device info (key-value text)
    system.physmem.store1.pmem   raw guest physical memory (already extracted)

Output (written into the same directory):
    m5.cpt                       gem5 checkpoint file (INI-like, rendered via Jinja2)
"""

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

import jinja2

# ---------------------------------------------------------------------------
# Register mappings (from reg_mappings.py)
# ---------------------------------------------------------------------------

miscreg_map={'dbgdtrrxext': 'osdtrrx_el1', 'dbgdscrext': 'mdscr_el1', 'dbgdtrtxext': 'osdtrtx_el1', 'dbgoseccr': 'oseccr_el1', 'dbgbxvr0': 'dbgbvr0_el1', 'dbgbxvr1': 'dbgbvr1_el1', 'dbgbxvr2': 'dbgbvr2_el1', 'dbgbxvr3': 'dbgbvr3_el1', 'dbgbxvr4': 'dbgbvr4_el1', 'dbgbxvr5': 'dbgbvr5_el1', 'dbgbxvr6': 'dbgbvr6_el1', 'dbgbxvr7': 'dbgbvr7_el1', 'dbgbxvr8': 'dbgbvr8_el1', 'dbgbxvr9': 'dbgbvr9_el1', 'dbgbxvr10': 'dbgbvr10_el1', 'dbgbxvr11': 'dbgbvr11_el1', 'dbgbxvr12': 'dbgbvr12_el1', 'dbgbxvr13':
        'dbgbvr13_el1', 'dbgbxvr14': 'dbgbvr14_el1', 'dbgbxvr15': 'dbgbvr15_el1', 'dbgbcr0': 'dbgbcr0_el1', 'dbgbcr1': 'dbgbcr1_el1', 'dbgbcr2': 'dbgbcr2_el1', 'dbgbcr3': 'dbgbcr3_el1', 'dbgbcr4': 'dbgbcr4_el1', 'dbgbcr5': 'dbgbcr5_el1', 'dbgbcr6': 'dbgbcr6_el1', 'dbgbcr7': 'dbgbcr7_el1', 'dbgbcr8': 'dbgbcr8_el1', 'dbgbcr9': 'dbgbcr9_el1', 'dbgbcr10': 'dbgbcr10_el1', 'dbgbcr11': 'dbgbcr11_el1', 'dbgbcr12': 'dbgbcr12_el1', 'dbgbcr13': 'dbgbcr13_el1', 'dbgbcr14': 'dbgbcr14_el1', 'dbgbcr15':
        'dbgbcr15_el1', 'dbgwvr0': 'dbgwvr0_el1', 'dbgwvr1': 'dbgwvr1_el1', 'dbgwvr2': 'dbgwvr2_el1', 'dbgwvr3': 'dbgwvr3_el1', 'dbgwvr4': 'dbgwvr4_el1', 'dbgwvr5': 'dbgwvr5_el1', 'dbgwvr6': 'dbgwvr6_el1', 'dbgwvr7': 'dbgwvr7_el1', 'dbgwvr8': 'dbgwvr8_el1', 'dbgwvr9': 'dbgwvr9_el1', 'dbgwvr10': 'dbgwvr10_el1', 'dbgwvr11': 'dbgwvr11_el1', 'dbgwvr12': 'dbgwvr12_el1', 'dbgwvr13': 'dbgwvr13_el1', 'dbgwvr14': 'dbgwvr14_el1', 'dbgwvr15': 'dbgwvr15_el1', 'dbgwcr0': 'dbgwcr0_el1', 'dbgwcr1':
        'dbgwcr1_el1', 'dbgwcr2': 'dbgwcr2_el1', 'dbgwcr3': 'dbgwcr3_el1', 'dbgwcr4': 'dbgwcr4_el1', 'dbgwcr5': 'dbgwcr5_el1', 'dbgwcr6': 'dbgwcr6_el1', 'dbgwcr7': 'dbgwcr7_el1', 'dbgwcr8': 'dbgwcr8_el1', 'dbgwcr9': 'dbgwcr9_el1', 'dbgwcr10': 'dbgwcr10_el1', 'dbgwcr11': 'dbgwcr11_el1', 'dbgwcr12': 'dbgwcr12_el1', 'dbgwcr13': 'dbgwcr13_el1', 'dbgwcr14': 'dbgwcr14_el1', 'dbgwcr15': 'dbgwcr15_el1', 'dbgdscrint': 'mdccsr_el0', 'dbgvcr': 'dbgvcr32_el2', 'dbgdrar': 'mdrar_el1',
        'dbgoslar': 'oslar_el1', 'dbgoslsr': 'oslsr_el1', 'dbgosdlr': 'osdlr_el1', 'dbgprcr': 'dbgprcr_el1', 'dbgclaimset': 'dbgclaimset_el1', 'dbgclaimclr': 'dbgclaimclr_el1', 'dbgauthstatus': 'dbgauthstatus_el1', 'id_pfr0': 'id_pfr0_el1', 'id_pfr1': 'id_pfr1_el1', 'id_dfr0': 'id_dfr0_el1', 'id_afr0': 'id_afr0_el1', 'id_mmfr0': 'id_mmfr0_el1', 'id_mmfr1': 'id_mmfr1_el1', 'id_mmfr2': 'id_mmfr2_el1', 'id_mmfr3': 'id_mmfr3_el1', 'id_mmfr4': 'id_mmfr4_el1', 'id_isar0': 'id_isar0_el1',
        'id_isar1': 'id_isar1_el1', 'id_isar2': 'id_isar2_el1', 'id_isar3': 'id_isar3_el1', 'id_isar4': 'id_isar4_el1', 'id_isar5': 'id_isar5_el1', 'id_isar6': 'id_isar6_el1', 'csselr_ns': 'csselr_el1', 'vpidr': 'vpidr_el2', 'vmpidr': 'vmpidr_el2', 'sctlr_ns': 'sctlr_el1', 'actlr_ns': 'actlr_el1', 'cpacr': 'cpacr_el1', 'hsctlr': 'sctlr_el2', 'hactlr': 'actlr_el2', 'hcr2': 'hcr_el2', 'hdcr': 'mdcr_el2', 'hcptr': 'cptr_el2', 'hstr': 'hstr_el2', 'hacr': 'hacr_el2', 'scr': 'scr_el3',
        'sder': 'sder32_el3', 'sdcr': 'mdcr_el3', 'ttbr0_ns': 'ttbr0_el1', 'ttbr1_ns': 'ttbr1_el1', 'ttbcr_ns': 'tcr_el1', 'httbr': 'ttbr0_el2', 'htcr': 'tcr_el2', 'vttbr': 'vttbr_el2', 'vtcr': 'vtcr_el2', 'dacr_ns': 'dacr32_el2', 'spsr_svc': 'spsr_el1', 'spsr_hyp': 'spsr_el2', 'spsr_mon': 'spsr_el3', 'adfsr_ns': 'afsr0_el1', 'aifsr_ns': 'afsr1_el1', 'ifsr_ns': 'ifsr32_el2', 'hadfsr': 'afsr0_el2', 'haifsr': 'afsr1_el2', 'hsr': 'esr_el2', 'fpexc': 'fpexc32_el2', 'ifar_ns': 'far_el1', 'hifar':
        'far_el2', 'hpfar': 'hpfar_el2', 'par_ns': 'par_el1', 'pmintenset': 'pmintenset_el1', 'pmintenclr': 'pmintenclr_el1', 'pmcr': 'pmcr_el0', 'pmcntenset': 'pmcntenset_el0', 'pmcntenclr': 'pmcntenclr_el0', 'pmovsclr': 'pmovsclr_el0', 'pmswinc': 'pmswinc_el0', 'pmselr': 'pmselr_el0', 'pmceid0': 'pmceid0_el0', 'pmceid1': 'pmceid1_el0', 'pmccntr': 'pmccntr_el0', 'pmxevtyper': 'pmxevtyper_el0', 'pmxevcntr': 'pmxevcntr_el0', 'pmuserenr': 'pmuserenr_el0', 'pmovsset': 'pmovsset_el0',
        'nmrr_ns': 'mair_el1', 'amair1_ns': 'amair_el1', 'hmair1': 'mair_el2', 'hamair1': 'amair_el2', 'vbar_ns': 'vbar', 'hvbar': 'vbar_el2', 'contextidr_ns': 'contextidr_el1', 'tpidrprw_ns': 'tpidr_el1', 'tpidrurw_ns': 'tpidr_el0', 'tpidruro_ns': 'tpidrro_el0', 'htpidr': 'tpidr_el2', 'cntfrq': 'cntfrq_el0', 'cntpct': 'cntpct_el0', 'cntvct': 'cntvct_el0', 'cntp_ctl_ns': 'cntp_ctl_el02', 'cntp_cval_ns': 'cntp_cval_el02', 'cntp_tval_ns': 'cntp_tval_el02', 'cntv_ctl':
        'cntv_ctl_el02', 'cntv_cval': 'cntv_cval_el02', 'cntv_tval': 'cntv_tval_el02', 'cntkctl': 'cntkctl_el1', 'cnthctl': 'cnthctl_el2', 'cnthp_ctl': 'cnthp_ctl_el2', 'cnthp_cval': 'cnthp_cval_el2', 'cnthp_tval': 'cnthp_tval_el2', 'cntvoff': 'cntvoff_el2', 'pmevcntr0': 'pmevcntr0_el0', 'pmevcntr1': 'pmevcntr1_el0', 'pmevcntr2': 'pmevcntr2_el0', 'pmevcntr3': 'pmevcntr3_el0', 'pmevcntr4': 'pmevcntr4_el0', 'pmevcntr5': 'pmevcntr5_el0', 'pmevtyper0': 'pmevtyper0_el0', 'pmevtyper1':
        'pmevtyper1_el0', 'pmevtyper2': 'pmevtyper2_el0', 'pmevtyper3': 'pmevtyper3_el0', 'pmevtyper4': 'pmevtyper4_el0', 'pmevtyper5': 'pmevtyper5_el0', 'icc_pmr': 'icc_pmr_el1', 'icc_iar0': 'icc_iar0_el1', 'icc_eoir0': 'icc_eoir0_el1', 'icc_hppir0': 'icc_hppir0_el1', 'icc_bpr0': 'icc_bpr0_el1', 'icc_ap0r0': 'icc_ap0r0_el1', 'icc_ap0r1': 'icc_ap0r1_el1', 'icc_ap0r2': 'icc_ap0r2_el1', 'icc_ap0r3': 'icc_ap0r3_el1', 'icc_ap1r0': 'icc_ap1r0_el1', 'icc_ap1r0_ns': 'icc_ap1r0_el1_ns',
        'icc_ap1r0_s': 'icc_ap1r0_el1_s', 'icc_ap1r1': 'icc_ap1r1_el1', 'icc_ap1r1_ns': 'icc_ap1r1_el1_ns', 'icc_ap1r1_s': 'icc_ap1r1_el1_s', 'icc_ap1r2': 'icc_ap1r2_el1', 'icc_ap1r2_ns': 'icc_ap1r2_el1_ns', 'icc_ap1r2_s': 'icc_ap1r2_el1_s', 'icc_ap1r3': 'icc_ap1r3_el1', 'icc_ap1r3_ns': 'icc_ap1r3_el1_ns', 'icc_ap1r3_s': 'icc_ap1r3_el1_s', 'icc_dir': 'icc_dir_el1', 'icc_rpr': 'icc_rpr_el1', 'icc_sgi1r': 'icc_sgi1r_el1', 'icc_asgi1r': 'icc_asgi1r_el1', 'icc_sgi0r': 'icc_sgi0r_el1', 'icc_iar1':
        'icc_iar1_el1', 'icc_eoir1': 'icc_eoir1_el1', 'icc_hppir1': 'icc_hppir1_el1', 'icc_bpr1': 'icc_bpr1_el1', 'icc_bpr1_ns': 'icc_bpr1_el1_ns', 'icc_bpr1_s': 'icc_bpr1_el1_s', 'icc_ctlr': 'icc_ctlr_el1', 'icc_ctlr_ns': 'icc_ctlr_el1_ns', 'icc_ctlr_s': 'icc_ctlr_el1_s', 'icc_sre': 'icc_sre_el1', 'icc_sre_ns': 'icc_sre_el1_ns', 'icc_sre_s': 'icc_sre_el1_s', 'icc_igrpen0': 'icc_igrpen0_el1', 'icc_igrpen1': 'icc_igrpen1_el1', 'icc_igrpen1_ns': 'icc_igrpen1_el1_ns',
        'icc_igrpen1_s': 'icc_igrpen1_el1_s', 'icc_hsre': 'icc_sre_el2', 'icc_mctlr': 'icc_ctlr_el3', 'icc_msre': 'icc_sre_el3', 'icc_mgrpen1': 'icc_igrpen1_el3', 'ich_ap0r0': 'ich_ap0r0_el2', 'ich_ap0r1': 'ich_ap0r1_el2', 'ich_ap0r2': 'ich_ap0r2_el2', 'ich_ap0r3': 'ich_ap0r3_el2', 'ich_ap1r0': 'ich_ap1r0_el2', 'ich_ap1r1': 'ich_ap1r1_el2', 'ich_ap1r2': 'ich_ap1r2_el2', 'ich_ap1r3': 'ich_ap1r3_el2', 'ich_hcr': 'ich_hcr_el2', 'ich_vtr': 'ich_vtr_el2', 'ich_misr': 'ich_misr_el2',
        'ich_eisr': 'ich_eisr_el2', 'ich_elrsr': 'ich_elrsr_el2', 'ich_vmcr': 'ich_vmcr_el2', 'ich_lr0': 'ich_lrc0', 'ich_lr1': 'ich_lrc1', 'ich_lr2': 'ich_lrc2', 'ich_lr3': 'ich_lrc3', 'ich_lr4': 'ich_lrc4', 'ich_lr5': 'ich_lrc5', 'ich_lr6': 'ich_lrc6', 'ich_lr7': 'ich_lrc7', 'ich_lr8': 'ich_lrc8', 'ich_lr9': 'ich_lrc9', 'ich_lr10': 'ich_lrc10', 'ich_lr11': 'ich_lrc11', 'ich_lr13': 'ich_lrc13', 'ich_lr14': 'ich_lrc14', 'ich_lr15': 'ich_lrc15'}

intreg_list=[
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x8", "x9", "x10", "x11", "x12", "x13", "x14", "0",
    "x19", "x18", "0", "0", "x15", "x21", "x20", "x23",
    "x22", "x17", "x16", "x24", "x25", "x26", "x27", "x28",
    "x29", "x30"
]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def _hex_to_int(hex_str):
    """Convert a hex string (with or without '0x' prefix) to an integer."""
    return int(hex_str, 16)


# ---------------------------------------------------------------------------
# Input parsing
# ---------------------------------------------------------------------------

def parse_register_json(json_path, cpu_index=0):
    """Parse register-info.json and return a reg_map dict for a single CPU.

    The JSON structure (produced by savevm.c / gdb_get_registers_qdict):
        {
            "format": "qemu-register-dump",
            "cpus": [
                {
                    "cpu_id": 0,
                    "registers": {
                        "x0": "0x...",
                        "pc": "0x...",
                        "q0": "0x...",
                        ...
                    }
                },
                ...
            ]
        }

    Register values are hex strings ("0x...").
    Returns the same dict[str, int] layout that the original QPoints
    ``parse_reg_info()`` produces.
    """
    with open(json_path, "r") as fh:
        data = json.load(fh)

    cpus = data["cpus"]
    if cpu_index >= len(cpus):
        raise ValueError(
            f"cpu_index {cpu_index} out of range (JSON has {len(cpus)} CPUs)"
        )

    regs_raw = cpus[cpu_index]["registers"]

    reg_map = {}
    for name, hex_val in regs_raw.items():
        reg_map[name.lower()] = _hex_to_int(hex_val)

    return reg_map


def parse_dev_info(dev_info_path, reg_map):
    """Parse dev.info and add vio_base / queue0_offset into *reg_map* (in-place).

    File format (one key-value pair per line):
        vio_base 0xa0000000
        queue0_offset 256
    """
    with open(dev_info_path, "r") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            toks = line.split()
            reg_map[toks[0]] = toks[1]


# ---------------------------------------------------------------------------
# Register correction (mirrors QPoints parse_reg_info.fix_sp_regs)
# ---------------------------------------------------------------------------

def fix_sp_regs(reg_map):
    """Apply the same SP / MAIR corrections that QPoints does."""
    # Assign 'sp' to the correct exception-level stack pointer
    if int(reg_map["cpsr"]) % 2 == 0:
        reg_map["sp_el0"] = reg_map["sp"]
    else:
        reg_map["sp_el1"] = reg_map["sp"]

    # Split the 64-bit MAIR register into two 32-bit halves for gem5
    mair_reg = int(reg_map["mair_el1"])
    reg_map["nmrr_ns"] = mair_reg >> 32


# ---------------------------------------------------------------------------
# Formatting helpers (mirrors QPoints parse_reg_info)
# ---------------------------------------------------------------------------

def reverse_byte_order(hex_str):
    """Reverse byte order of a 512-character hex string (256 bytes)."""
    assert len(hex_str) == 512
    rev = ""
    for i in range(255, 0, -1):
        rev += hex_str[2 * i : 2 * i + 2]
    return rev


def get_intreg_string(reg_map):
    """Build the 43-element integer register string (space-separated decimals)."""
    TOTAL_REGS = 43
    SP_REGS = 4
    NUM_XREGS = len(intreg_list)  # 34

    int_regs = []
    for reg in intreg_list:
        if reg == "0":
            int_regs.append(0)
        else:
            int_regs.append(reg_map[reg])

    filler_len = TOTAL_REGS - SP_REGS - NUM_XREGS
    int_regs.extend([0] * filler_len)

    for el_level in range(SP_REGS):
        key = f"sp_el{el_level}"
        int_regs.append(reg_map.get(key, 0))

    return " ".join(map(str, int_regs))


def get_fpreg_string(reg_map):
    """Build the 43-element vector register string (space-separated 512-char hex).

    The JSON from gdb_get_registers_qdict uses 'v0'-'v31' (from aarch64-fpu.xml),
    while the original QPoints GDB text dump used 'q0'-'q31'.  We try 'v' first,
    then fall back to 'q' for compatibility.
    """
    TOTAL_REGS = 43
    NUM_FPREGS = 32

    fp_regs = []
    for i in range(NUM_FPREGS):
        if f"v{i}" in reg_map:
            fp_regs.append(reg_map[f"v{i}"])
        else:
            fp_regs.append(reg_map[f"q{i}"])

    fp_regs.extend([0] * (TOTAL_REGS - NUM_FPREGS))

    return " ".join(
        reverse_byte_order("{:0512x}".format(v)) for v in fp_regs
    )


def get_cc_reg_string(cpsr):
    """Extract NZCV condition-code fields from CPSR."""
    nzcv = cpsr >> 28
    nz = nzcv >> 2
    c = (nzcv >> 1) & 0x01
    v = nzcv & 0x01
    return f"{nz} {c} {v} 0 0 0"


def get_miscreg_output(miscreg_ref_path, reg_map):
    """Build the misc-register KEY=VALUE block for the m5.cpt template.

    *miscreg_ref_path* is the gem5_misc_regs reference file that lists every
    register with a default value. Actual captured values from *reg_map*
    override the defaults.
    """
    lines = []
    with open(miscreg_ref_path, "r") as fh:
        for line in fh:
            toks = line.split("=")
            reg_name = toks[0]
            reg_val = toks[1].strip()

            if reg_name in reg_map:
                reg_val = reg_map[reg_name]
            elif reg_name in miscreg_map and miscreg_map[reg_name] in reg_map:
                reg_val = reg_map[miscreg_map[reg_name]]
            else:
                _eprint(f"{reg_name} not in reg_map")

            lines.append(f"{reg_name}={reg_val}")

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Checkpoint generation
# ---------------------------------------------------------------------------

def generate_m5_cpt(gem_dir, num_cores, template_name=None):
    """Generate m5.cpt inside *gem_dir* from register-info.json + dev.info.

    Parameters
    ----------
    gem_dir : str | Path
        Path to the ``<snapshot>.gem/`` directory created by savevm.c.
    num_cores : int
        Number of CPU cores present in the register dump.
    template_name : str, optional
        Override for the Jinja2 template filename.  Defaults to
        ``m5.cpt.template.j2`` for single-core,
        ``m5.cpt.multicore.template.j2`` for multi-core.
    """
    gem_dir = Path(gem_dir)
    script_dir = Path(__file__).resolve().parent
    # Templates are always read from /home/dev/qflex/templates/
    templates_dir = Path("/home/dev/qflex/templates")

    json_path = gem_dir / "register-info.json"
    dev_info_path = gem_dir / "dev.info"
    miscreg_ref = script_dir / "gem5_misc_regs"
    out_path = gem_dir / "m5.cpt"

    if template_name is None:
        if num_cores == 1:
            template_name = "m5.cpt.template.j2"
        else:
            template_name = "m5.cpt.multicore.template.j2"

    template_path = templates_dir / template_name

    if num_cores == 1:
        reg_map = parse_register_json(json_path, cpu_index=0)
        parse_dev_info(dev_info_path, reg_map)
        fix_sp_regs(reg_map)

        miscreg_str = get_miscreg_output(str(miscreg_ref), reg_map)
        intreg_str = get_intreg_string(reg_map)
        fpreg_str = get_fpreg_string(reg_map)
        ccreg_str = get_cc_reg_string(reg_map["cpsr"])

        pc = reg_map["pc"]
        npc = pc + 4

        rendered = (
            jinja2.Environment(loader=jinja2.FileSystemLoader("/"))
            .get_template(str(template_path))
            .render(
                pc=pc,
                npc=npc,
                miscreg_string=miscreg_str,
                intreg_string=intreg_str,
                fpreg_string=fpreg_str,
                ccreg_string=ccreg_str,
                reg_map=reg_map,
            )
        )
    else:
        reg_map = [None] * num_cores
        miscreg_str = [None] * num_cores
        intreg_str = [None] * num_cores
        fpreg_str = [None] * num_cores
        ccreg_str = [None] * num_cores
        pc = [None] * num_cores
        npc = [None] * num_cores

        for i in range(num_cores):
            reg_map[i] = parse_register_json(json_path, cpu_index=i)
            parse_dev_info(dev_info_path, reg_map[i])
            fix_sp_regs(reg_map[i])

            miscreg_str[i] = get_miscreg_output(str(miscreg_ref), reg_map[i])
            intreg_str[i] = get_intreg_string(reg_map[i])
            fpreg_str[i] = get_fpreg_string(reg_map[i])
            ccreg_str[i] = get_cc_reg_string(reg_map[i]["cpsr"])

            pc[i] = reg_map[i]["pc"]
            npc[i] = pc[i] + 4

        rendered = (
            jinja2.Environment(loader=jinja2.FileSystemLoader("/"))
            .get_template(str(template_path))
            .render(
                pc=pc,
                npc=npc,
                miscreg_string=miscreg_str,
                intreg_string=intreg_str,
                fpreg_string=fpreg_str,
                ccreg_string=ccreg_str,
                reg_map=reg_map,
                num_cores=num_cores,
            )
        )

    with open(out_path, "w") as fh:
        fh.write(rendered)
    _eprint(f"[gem5_chkpt] m5.cpt written to {out_path}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate gem5 m5.cpt checkpoint from QEMU savevm artefacts"
    )
    parser.add_argument(
        "gem_dir",
        help="Path to the <snapshot>.gem/ directory containing "
             "register-info.json, dev.info, and system.physmem.store1.pmem",
    )
    parser.add_argument(
        "--num-cores",
        type=int,
        default=1,
        help="Number of CPU cores (default: 1)",
    )
    parser.add_argument(
        "--template",
        default=None,
        help="Override Jinja2 template filename (looked up in templates/)",
    )

    args = parser.parse_args()
    generate_m5_cpt(args.gem_dir, args.num_cores, args.template)


if __name__ == "__main__":
    main()
