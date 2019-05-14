import os
import sys
import string
import argparse
import subprocess

def deploy_system(arguments):
    parser = argparse.ArgumentParser(description = "Deploys a qflex job using kubernetes")
    parser.add_argument("--job_name",               help = "Set name for the job")
    parser.add_argument("--step_name",              help = "Set name for the step of the job")
    parser.add_argument("--docker_image_name",      help = "Set the name of the docker image")
    parser.add_argument("--config_name",            help = "Set the name of the config file")
    parser.add_argument("--host_flexus_path",       help = "Set the path to the Flexus repository on the local file system")
    parser.add_argument("--host_image_repo_path",   help = "Set the path to the images repository on the local file system")
    parser.add_argument("--host_workspace",         help = "Set the path to the system config file on the local file system")
    parser.add_argument("--target_flexus_path",     help = "Set the path to the Flexus repository in the docker image")
    parser.add_argument("--target_image_repo_path", help = "Set the path to the images repository in the docker image")
    parser.add_argument("--target_workspace",       help = "Set the path to the system config file in the docker image")

    args = parser.parse_args(arguments)

    username               = os.getlogin()
    userid                 = os.getuid()
    groupid                = os.getgid()
    job_name               = args.job_name
    step_name              = args.step_name
    docker_image_name      = args.docker_image_name
    target_image_repo_path = args.target_image_repo_path
    target_flexus_path     = args.target_flexus_path
    target_workspace       = args.target_workspace
    host_image_repo_path   = args.host_image_repo_path
    host_flexus_path       = args.host_flexus_path
    host_workspace         = args.host_workspace
    config_path            = os.path.join(args.target_workspace, args.config_name)

    yaml_text = string.Template(
"""apiVersion: batch/v1
kind: Job
metadata:
  generateName: $username-
  labels:
    user: $username
    jname: $job_name
spec:
  backoffLimit: 3
  template:
    metadata:
      generateName: $username-
    spec:
      containers:
      - name: $username-$job_name-$step_name
        image: $docker_image_name
        volumeMounts:
        - mountPath: $target_image_repo_path
          name: volume-images
        - mountPath: $target_flexus_path
          name: volume-flexus
        - mountPath: $target_workspace
          name: volume-workspace
        imagePullPolicy: IfNotPresent
        command: ["/usr/local/bin/entrypoint.sh"]
        args: ["python","/home/user/qflex/scripts/run_system.py", "$config_path"]
        env:
        - name: LOCAL_USER_ID
          value: "$userid"
        - name: LOCAL_GROUP_ID
          value: "$groupid"
      restartPolicy: OnFailure
      volumes:
      - name: volume-images
        hostPath:
          path: $host_image_repo_path
          type: Directory
      - name: volume-flexus
        hostPath:
          path: $host_flexus_path
          type: Directory
      - name: volume-workspace
        hostPath:
          path: $host_workspace
          type: Directory"""
    )

    yaml_name = job_name + "-" + step_name + ".yaml"
    f = open(yaml_name, "w+")
    f.write(yaml_text.substitute(locals()))
    f.close()

    subprocess.call(["kubectl", "create", "-f", yaml_name])
    os.remove(yaml_name)

if __name__ == '__main__':
    deploy(sys.argv[1:])