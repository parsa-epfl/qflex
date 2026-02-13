import os
import yaml
import boto3
from typing import Dict, Any
from commands import Executor
from commands.s3 import load_s3_credentials, load_workload_config


class ListWorkloads(Executor):

    def __init__(self,
                 config_file: str = "./workloads.yaml",
                 s3_config_file: str = "./s3-config.yaml"):
        self.config_file = config_file
        self.s3_config_file = s3_config_file
        self.s3_credentials = load_s3_credentials(s3_config_file)

    def _get_available_workloads(self) -> list:
        workloads = load_workload_config(self.config_file, workload_name=None)
        
        # Initialize S3 client
        s3_config = {
            'aws_access_key_id': self.s3_credentials['aws_access_key_id'],
            'aws_secret_access_key': self.s3_credentials['aws_secret_access_key'],
            'region_name': self.s3_credentials['region'],
            'endpoint_url': self.s3_credentials['endpoint_url']
        }
        s3_client = boto3.client('s3', **s3_config)
        
        results = []
        
        for workload in workloads:
            workload_name = workload['workload']
            
            # Construct S3 path
            bucket_name = workload['s3']['bucket_name']
            remote_path = workload['s3']['remote_path']
            version = workload['version']
            image_name = workload['image']['name']
            
            s3_key = f"{remote_path.rstrip('/')}/{version}/{image_name}"
            
            # Check if object exists
            try:
                s3_client.head_object(Bucket=bucket_name, Key=s3_key)
                results.append((workload_name, version, True, None))
            except s3_client.exceptions.NoSuchKey:
                results.append((workload_name, version, False, "not found"))
            except Exception as e:
                results.append((workload_name, version, False, str(e)))
        
        return results

    def cmd(self) -> str:
        workloads = self._get_available_workloads()
        
        print("Available workloads in S3:\n")
        
        available_workloads = []
        for workload_name, version, exists, error in workloads:
            if exists:
                available_workloads.append(f"{workload_name}:{version}")
                print(f"  ✓ {workload_name}:{version}")
            else:
                print(f"  ✗ {workload_name}:{version} ({error})")
                        
        return []
