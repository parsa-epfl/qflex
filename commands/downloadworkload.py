import os
import yaml
import boto3
import hashlib
from typing import Dict, Any, Optional
from tqdm import tqdm
from commands import Executor
from commands.jinja_loaders.qflex_args_loader import QflexArgsLoader
from commands.s3 import load_s3_credentials, load_workload_config, validate_workload_config


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
        self.config = load_workload_config(config_file, workload_name)
        self.s3_credentials = load_s3_credentials(s3_config_file)

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
