import os
import sys
import string
import argparse
import subprocess

def deploy(arguments):
    parser = argparse.ArgumentParser(description = "Deploys a qflex job using kubernetes")
    parser.add_argument("--job_name",               help = "Set name for the job")
    parser.add_argument("--step_name",              help = "Set name for the step of the job")
    parser.add_argument("--docker_image_name",      help = "Set the name of the docker image")
    parser.add_argument("--local_flexus_path",      help = "Set the path to the Flexus repository on the local file system")
    parser.add_argument("--local_image_repo_path",  help = "Set the path to the images repository on the local file system")
    parser.add_argument("--local_xml_path",         help = "Set the path to the directory that contains all required xml files for mrun, with 'setup_file.xml' as the setup file")
    parser.add_argument("--flexus_path",            help = "Set the path to the Flexus repository in the docker image")
    parser.add_argument("--image_repo_path",        help = "Set the path to the images repository in the docker image")
    parser.add_argument("--xml_path",               help = "Set the path to the xml directory in the docker image")

    args = parser.parse_args(arguments)

    username              = os.getlogin()
    userid                = os.getuid()
    groupid               = os.getgid()
    step_name             = args.step_name
    job_name              = args.job_name
    docker_image_name     = args.docker_image_name
    image_repo_path       = args.image_repo_path
    flexus_path           = args.flexus_path
    xml_path              = args.xml_path
    local_image_repo_path = args.local_image_repo_path
    local_flexus_path     = args.local_flexus_path
    local_xml_path        = args.local_xml_path

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
        - mountPath: $image_repo_path
          name: volume-images
        - mountPath: $flexus_path
          name: volume-flexus
        - mountPath: $xml_path
          name: volume-xml
        imagePullPolicy: IfNotPresent
        command: ["/usr/local/bin/entrypoint.sh"]
        args: ["python","/home/user/qflex/scripts/run_job/mrun/mrun.py","-r","/home/user/xml/setup_file.xml","-l","DEBUG"]
        env:
        - name: LOCAL_USER_ID
          value: "$userid"
        - name: LOCAL_GROUP_ID
          value: "$groupid"
      restartPolicy: OnFailure
      volumes:
      - name: volume-images
        hostPath:
          path: $local_image_repo_path
          type: Directory
      - name: volume-flexus
        hostPath:
          path: $local_flexus_path
          type: Directory
      - name: volume-xml
        hostPath:
          path: $local_xml_path
          type: Directory"""
    )

    yaml_name = job_name + "-" + step_name + ".yaml"
    f = open(yaml_name, "w+")
    f.write(yaml_text.substitute(locals()))
    f.close()

    subprocess.call(["kubectl", "create", "-f", yaml_name])
    subprocess.call(["rm", yaml_name])

## Execute script
if __name__ == '__main__':
    deploy(sys.argv[1:])