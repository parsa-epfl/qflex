from typing import Dict, Any
import os
import jinja2


class QflexArgsLoader:

    def __init__(self, 
                 config_dict: Dict[str, Any],
                 workload_name: str,
                 output_path: str = "./qflex.args"):
        self.config_dict = config_dict
        self.workload_name = workload_name
        self.output_path = os.path.abspath(output_path)
        self.parameter_template = 'qflex.args.j2'
        
    def load_parameters(self) -> str:
        jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader("./templates"))
        template = jinja_env.get_template(self.parameter_template)
        context = self.get_context()
        template.stream(context).dump(self.output_path)
        return self.output_path
    
    def get_context(self) -> Dict[str, Any]:
        config = self.config_dict
        return {
            'CORE_COUNT': config['resources']['cores']['count'],
            'DOUBLE_CORES_FLAG': '--double-cores' if config['resources']['cores'].get('double_cores', False) else '--no-double-cores',
            'QUANTUM_SIZE_NS': config['simulation']['quantum']['quantum_size_ns'],
            'LLC_SIZE_PER_TILE_MB': config['resources']['llc']['size_per_tile_mb'],
            'PARALLEL_FLAG': '--parallel' if config['simulation'].get('parallel', True) else '--no-parallel',
            'NETWORK': config['resources']['network']['type'],
            'MEMORY_GB': config['resources']['memory']['size_gb'],
            'HOST_NAME': config['resources']['host_name'],
            'WORKLOAD_NAME': self.workload_name,
            'POPULATION_SECONDS': config['simulation']['population'],
            'CONSOLIDATED_FLAG': '--consolidated' if config['simulation'].get('consolidated', False) else '--no-consolidated',
            'PRIMARY_IPC': config['simulation']['ipc']['primary_ipc'],
            'PRIMARY_CORE_START': config['simulation']['ipc']['primary_core_start'],
            'SECONDARY_IPC_ARG': f"--secondary-ipc {config['simulation']['ipc']['secondary_ipc']}" if config['simulation'].get('consolidated', False) and config['simulation']['ipc'].get('secondary_ipc') else '',
            'SECONDARY_CORE_START_ARG': f"--secondary-core-start {config['simulation']['ipc']['secondary_core_start']}" if config['simulation'].get('consolidated', False) and config['simulation']['ipc'].get('secondary_core_start') else '',
            'PHANTOM_CPU_IPC': config['simulation']['ipc']['phantom_cpu_ipc'],
            'EXPERIMENT_NAME': config['experiment']['name'],
            'IMAGE_NAME': config['image']['name'],
            'IMAGE_FOLDER': config['image']['folder'],
            'MOUNTING_FOLDER': config['mounting_directory'],
            'CHECK_PERIOD_QUANTUM_COEFF': config['simulation']['quantum']['check_period_quantum_coeff'],
            'MACHINE_FREQ_GHZ': config['resources']['machine_freq_ghz'],
            'UNIQUE_FLAG': '--unique' if config['experiment'].get('unique', False) else '--no-unique',
            'USE_IMAGE_DIRECTLY_FLAG': '--use-image-directly' if config['image'].get('use_image_directly', False) else '--no-use-image-directly',
        }
