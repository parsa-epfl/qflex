import os
import yaml
import boto3
import hashlib
from typing import Dict, Any, Optional
from tqdm import tqdm
from commands import Executor
from commands.jinja_loaders.qflex_args_loader import QflexArgsLoader



class DownloadWorkloadImage(Executor):

    def __init__(self,
                 workload_name: str,
                 image_folder: str,
                 config_file: str = "./workloads.yaml",
                 output_path: str = "./qflex.args",
                 s3_config_file: str = "./s3-config.yaml"):
        self.workload_name = workload_name
        self.image_folder = os.path.abspath(image_folder)
        self.config_file = config_file
        self.output_path = output_path
        self.s3_config_file = s3_config_file
        
        os.makedirs(self.image_folder, exist_ok=True)
        self.config = self._load_config()
        self._validate_config(self.config)
        self.s3_credentials = self._load_s3_credentials()
        
        
    def _load_s3_credentials(self) -> Dict[str, str]:

        if not os.path.exists(self.s3_config_file):
            raise FileNotFoundError(
                f"S3 config file not found: {self.s3_config_file}\n"
            )
        
        credentials = {}
        with open(self.s3_config_file, 'r') as f:
            credentials = yaml.safe_load(f)
        if not isinstance(credentials, dict):
            raise ValueError("S3 config file must contain a valid YAML dictionary")
        
        required_keys = ['aws_access_key_id', 'aws_secret_access_key', 'region', 'endpoint_url']
        for key in required_keys:
            if key not in credentials:
                raise ValueError(f"Missing required S3 credential: {key}")
        
        return credentials


    def _validate_config(self, config: Dict[str, Any]) -> None:
        """Recursively validate all configuration fields."""
        
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

        # Add consolidated-specific validation
        if config['simulation'].get('consolidated'):
            if 'secondary_ipc' not in config['simulation']['ipc']:
                raise ValueError("Missing required key: simulation.ipc.secondary_ipc (required when consolidated=True)")
            if not isinstance(config['simulation']['ipc']['secondary_ipc'], (int, float)):
                raise ValueError(f"simulation.ipc.secondary_ipc must be numeric")
            if 'secondary_core_start' not in config['simulation']['ipc']:
                raise ValueError("Missing required key: simulation.ipc.secondary_core_start (required when consolidated=True)")
            if not isinstance(config['simulation']['ipc']['secondary_core_start'], int):
                raise ValueError(f"simulation.ipc.secondary_core_start must be integer")


    def _load_config(self):

        if not os.path.exists(self.config_file):
            raise FileNotFoundError(f"Configuration file not found: {self.config_file}")
        
        with open(self.config_file, 'r') as f:
            yaml_data = yaml.safe_load(f)

        if 'default' not in yaml_data:
            raise ValueError("Configuration file must contain a 'default' section")
        default_config = yaml_data['default']

        if 'workloads' not in yaml_data:
            raise ValueError("Configuration file must contain a 'workloads' section")
        workload_override = None
        for workload_entry in yaml_data['workloads']:
            if workload_entry.get('workload') == self.workload_name:
                workload_override = workload_entry
                break
        if workload_override is None:
            raise ValueError(f"Workload '{self.workload_name}' not found in configuration file")
        workload_override.pop('workload', None)

        def _deep_merge(self, base: Dict[Any, Any], override: Dict[Any, Any]) -> Dict[Any, Any]:
            result = base.copy()        
            for key, value in override.items():
                if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                    result[key] = self._deep_merge(result[key], value)
                else:
                    result[key] = value
            return result

        merged_config = self._deep_merge(default_config, workload_override)
        return merged_config


    def _generate_qflex_args(self):
        args_loader = QflexArgsLoader(
            config_dict=self.config,
            workload_name=self.workload_name,
            output_path=self.output_path
        )
        args_loader.load_parameters()
    

    def _calculate_file_hash(self, filepath: str, algorithm: str = 'md5') -> str:
        """Calculate hash of a file for integrity verification."""
        hash_func = hashlib.new(algorithm)
        with open(filepath, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                hash_func.update(chunk)
        return hash_func.hexdigest()
    
    def _download_from_s3(self, s3_client, bucket: str, remote_path: str, local_path: str):

        # Get file size for progress bar
        try:
            response = s3_client.head_object(Bucket=bucket, Key=remote_path)
            file_size = response['ContentLength']
            metadata = response.get('Metadata', {})
        except Exception as e:
            raise RuntimeError(f"Failed to get object info from S3: {e}")
        
        # Download with progress bar
        with tqdm(total=file_size, unit='B', unit_scale=True, desc=os.path.basename(local_path)) as pbar:
            def callback(bytes_transferred):
                pbar.update(bytes_transferred)
            try:
                s3_client.download_file(bucket, remote_path, local_path, Callback=callback)
            except Exception as e:
                raise RuntimeError(f"Failed to download from S3: {e}")        
        print(f"Workload '{self.workload_name}' downloaded to: {local_path}")
        
        # Verify integrity from metadata
        for hash_type in ['sha256', 'md5']:
            if hash_type in metadata:
                expected_hash = metadata[hash_type]
                actual_hash = self._calculate_file_hash(local_path, hash_type)
                
                if actual_hash == expected_hash:
                    print(f"Integrity verified ({hash_type.upper()})")
                else:
                    print(f"WARNING: {hash_type.upper()} checksum mismatch!")
                    print(f"Expected: {expected_hash}")
                    print(f"Got:      {actual_hash}")
                return
                

    def cmd(self) -> str:

        s3_config = {
            'aws_access_key_id': self.s3_credentials['aws_access_key_id'],
            'aws_secret_access_key': self.s3_credentials['aws_secret_access_key'],
            'region_name': self.s3_credentials['region'],
            'endpoint_url': self.s3_credentials['endpoint_url']
        }
        s3_client = boto3.client('s3', **s3_config)
        
        bucket_name = self.config.s3['bucket_name']
        remote_path = self.config.s3['remote_path']
        version = self.config['version']
        image_name = self.config.image['name']
        
        remote_path = f"{remote_path.rstrip('/')}/{version}/{image_name}"        
        local_image_path = os.path.join(self.image_folder, image_name)
                
        print(f"Downloading workload '{self.workload_name}' from S3...")
        print(f"  Bucket: {bucket_name}")
        print(f"  Remote: {remote_path}")
        print(f"  Local:  {local_image_path}")
        
        self._download_from_s3(s3_client, bucket_name, remote_path, local_image_path)
        self._generate_qflex_args()
        
        print(f"Generated qflex.args at: {self.output_path}")
                
        return []
