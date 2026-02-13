import os
import yaml
import copy
from typing import Dict, Any, Optional


def load_s3_credentials(s3_config_file: str) -> Dict[str, str]:
    if not os.path.exists(s3_config_file):
        raise FileNotFoundError(f"S3 config file not found: {s3_config_file}")
    
    credentials = {}
    with open(s3_config_file, 'r') as f:
        credentials = yaml.safe_load(f)
    if not isinstance(credentials, dict):
        raise ValueError("S3 config file must contain a valid YAML dictionary")
    
    required_keys = ['aws_access_key_id', 'aws_secret_access_key', 'region', 'endpoint_url']
    for key in required_keys:
        if key not in credentials:
            raise ValueError(f"Missing required S3 credential: {key}")
    
    return credentials


def deep_merge(base: Dict[Any, Any], override: Dict[Any, Any]) -> Dict[Any, Any]:
    """Deep merge two dictionaries, with override taking precedence."""
    result = copy.deepcopy(base)
    for key, value in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value
    return result


def load_workload_config(config_file: str, workload_name: Optional[str] = None) -> Dict[str, Any]:

    if not os.path.exists(config_file):
        raise FileNotFoundError(f"Configuration file not found: {config_file}")
    
    with open(config_file, 'r') as f:
        yaml_data = yaml.safe_load(f)
    
    if 'default' not in yaml_data:
        raise ValueError("Configuration file must contain a 'default' section")
    
    if 'workloads' not in yaml_data:
        raise ValueError("Configuration file must contain a 'workloads' section")
    
    # If no workload_name provided, return all workloads 
    if workload_name is None:
        default_config = yaml_data['default']
        merged_workloads = []
        
        for workload_entry in yaml_data['workloads']:
            merged_config = deep_merge(default_config, workload_entry)
            validate_workload_config(merged_config)
            merged_workloads.append(merged_config)
        
        return merged_workloads
    
    default_config = yaml_data['default']
    workload_override = None
    for workload_entry in yaml_data['workloads']:
        if workload_entry.get('workload') == workload_name:
            workload_override = workload_entry.copy()
            break
    if workload_override is None:
        raise ValueError(f"Workload '{workload_name}' not found in configuration file")
    workload_override.pop('workload', None)
    
    workload_config = deep_merge(default_config, workload_override)
    validate_workload_config(workload_config)

    return workload_config


def validate_workload_config(config: Dict[str, Any]) -> None:
    """Recursively validate all workload configuration fields."""
    
    required_structure = {
        'version': str,
        'experiment': {
            'name': str,
        },
        'image': {
            'name': str,
            'folder': str,
        },
        'mounting_directory': str,
        'resources': {
            'cores': {
                'count': int,
                'double_cores': bool
            },
            'llc': {
                'size_per_tile_mb': (int, float)
            },
            'memory': {
                'size_gb': int
            },
            'network': {
                'type': str
            },
            'machine_freq_ghz': (int, float),
        },
        'simulation': {
            'parallel': bool,
            'consolidated': bool,
            'population': int,
            'sample_size': int,
            'ipc': {
                'primary_ipc': (int, float),
                'primary_core_start': int,
                'phantom_cpu_ipc': (int, float)
            },
            'quantum': {
                'quantum_size_ns': int,
                'check_period_quantum_coeff': (int, float)
            }
        },
        's3': {
            'bucket_name': str,
            'remote_path': str
        }
    }
    
    def validate_recursive(expected, actual, path=""):
        for key, value_type in expected.items():
            full_path = f"{path}.{key}" if path else key

            if key not in actual:
                raise ValueError(f"Missing required key: {full_path}")
            if isinstance(value_type, dict):
                if not isinstance(actual[key], dict):
                    raise ValueError(f"{full_path} must be a dict")
                validate_recursive(value_type, actual[key], full_path)
            else:
                if not isinstance(actual[key], value_type):
                    raise ValueError(f"{full_path} must be {value_type}")
    validate_recursive(required_structure, config)

    # Validate consolidated-specific fields
    if config['simulation'].get('consolidated'):
        if 'secondary_ipc' not in config['simulation']['ipc']:
            raise ValueError("Missing required key: simulation.ipc.secondary_ipc (required when consolidated=True)")
        if not isinstance(config['simulation']['ipc']['secondary_ipc'], (int, float)):
            raise ValueError("simulation.ipc.secondary_ipc must be numeric")
        if 'secondary_core_start' not in config['simulation']['ipc']:
            raise ValueError("Missing required key: simulation.ipc.secondary_core_start (required when consolidated=True)")
        if not isinstance(config['simulation']['ipc']['secondary_core_start'], int):
            raise ValueError("simulation.ipc.secondary_core_start must be integer")
