# Captain

Captain includes a set of scripts to simplify launching simulations using QFlex.
`captain` is the top level script that launches a simulation
for a single or multiple QFlex instances.
It requires a configuration file, `system.ini`,
which in turn requires one or more configuration files, `instance.ini`,
one for each of the instances in the system.
`run_job.py` allows running a series of simulations that follow the SMARTS methodology.
It requires a configuration file, `job.ini`,
which is an extension of a system configuration file.
Sample configuration files can be found under `config`.
You can find more information on the configuration parameters in the sections below.

## Instance

An instance is the simulator building block unit,
and corresponds to a single QFlex simulated machine.
Each instance can be configured using an `ini` configuration file.
The configuration file is split to three sections:
`environment`, `machine` and `simulation`.

### Environment Parameters

These are parameters that define the environment variables and paths required
(on the host machine) for emulating the machine
(e.g., qflex repository path, image path, etc.).

* **qflex_path**: Path to the QFlex repository, used to locate QEMU executable.
* **image_repo_path**: Base path to the images directory.
All image-related files must be in the same directory for ease of deployment.
* **disk_image_path**: Path to the disk image relative to `image_repo_path`.
Flash images, `flash0.img` and `flash1.img`, must exist in the same path.
* **starting_snapshot**: Name of the starting snapshot, if any,
which is an external snapshot from the base disk image.

### Machine Parameters

Machine parameters indicate the QEMU configuration parameters for the emulated machine
(e.g., core count, memory size, network, external ports, etc.)
Details for QEMU parameters can be found
[here](https://qemu.weilnetz.de/doc/qemu-doc.html).

* **qemu_core_count**: Number of cores, integer value.
* **memory_size**: Size of memory, in bytes.
* **machine**: Machine parameters as specified in the documentation.
Assigned to `virt` unless otherwise required.
* **cpu**: CPU model. `cortex-a57` is currently supported.
* **user_network**: To enable user network by set this parameter to `on`,
otherwise it has to be set to `off` or omitted completely (along with the following parameters).
To configure the network, following parameters have to be set.
  * **user_network_id**: Name of the network id.
  * **user_network_mac**: Network mac address.
  * **user_network_hostfwd_protocol**: Network host forwarding protocol, as supported by QEMU.
  * **user_network_hostfwd_hostport**: Network host forwarding port.
  * **user_network_hostfwd_guestport**: Network guest forwarding port.
* **qmp**: Redirect external qmp monitor to given device.
If `qmp` is not needed this parameter can be set to `off`,
or can be omitted (along with the following parameters).
  * **qmp_dev**: Parameters for qmp device as specified in the documentation.
* **serial**: Redirect virtual serial port to given device.
If serial is not needed this parameter can be set to `off`,
or can be omitted (along with the following parameters).
  * **serial_dev**: Parameters for serial device as specified in the documentation.

### Simulation Parameters

These are the parameters needed for simulation,
including paths to the simulation-related files on the host machine
(e.g., flexus path, trace and timing simulators, etc.)
and simulation parameters.

* **flexus_path**: Base path to the `flexus` repository.
All flexus-related files must be in the same directory for ease of deployment.
* **flexus_trace_path**: Path to the trace simulator, relative to `flexus_path`.
* **flexus_timing_path**: Path to the timing simulator, relative to `flexus_path`.
* **user_postload_path**: Path to the flexus `user_postload` configuration file.
* **simulation_type**: Simulation type, `emulation`, `trace`, `timing`, `phases`, or `checkpoints`.
The following simulation parameters are required depending on the simulation type.
  * **simulation_length**: Length of the simulation for `trace` or `timing`.
  * **phases_name**: Name of the phases external snapshots created.
  * **phases_length**: The length of each phase separated by a colon,
  e.g., `1:2:30:4k:5m:6b`, where large numbers can be expressed using numerical representations.
  The number of phases is inferred by the number count in the value of the parameter.
  * **checkpoints_number**: Number of checkpoints.
  * **checkpoints_length**: The length of each checkpoint,
  where large numbers can be expressed using numerical representations.

## System

A system runs and controls a single or multiple instances of QFlex simulated machines.
A system has an `ini` configuration file which specifies the instances,
the system parameters, and the multi-node network parameters; each in a separate section.

### Instances

Each instance in the system must be specified under the `Instances` section
of the system configuration file.
The entry specifies the name of the instance and the configuration file of the instance.
Each instance must have a unique name, however, multiple instances can share
the same configuration file. An example of an instance entry is
`i0 = config/instance.ini`.

The entry shows an instance named `i0` based on a configuration file
`config/instance.ini`.
This relative path is resolved based on Python's working directory.

### System Parameters

These are instance parameters common across all instances of the system.
To prevent any mis-configuration, these parameters are set in the system config file.
Note that a system doesn't define how each instance is emulated.
This is left to be specified per instance, separately in their own instance config file.

* **icount**: Enable icount for instances by setting this parameter to `on`.
The below parameters must also be set.
This parameter can be set to `off` or can be omitted
(along with the following parameters) if icount is not needed.
More details can be found in QEMU documentation.
  * **icount_shift**: icount shift amount, in integers.
  * **icount_sleep**: Enable icount sleep mode by setting this parameter to `on`.
  Otherwise, setting it to `off` disables it.

### Multi-node

In order to set network connections between multiple instances,
we use an underlying network simulator, NS3.
This part is still a work in progress.
In this section, we configure the details of this simulator.

* **ns3_path**: Path to ns3 network simulator executable.
* **quantum**: Value of quantum per instance,
where large numbers can be expressed using numerical representations.

## Job
A job is a simulation following the SMARTS methodology,
which includes generating checkpoints for each phase,
and then running timing simulation for each checkpoint.
A job configuration file is an extension to the system configuration file.
It has the same sections with a few additional ones.
All the simulation details in all instance config files are ignored
when running a job as the job redefines these details in the job section.
It also adds a host section that allows Kubernetes deployment,
a feature we are currently working on.
Please note all instances in a job must have their flexus simulator
and images in the same directory for ease of deployment.

### Job Parameters

In a job, all instances are emulated except the master instance,
on which the SMARTS methodology is applied.
Three different types of simulation are performed on a system undergoing a job:
creating phases, generating checkpoints and simulating in timing.
The parameters needed for these simulations
(defined in the `simulation` section for an `instance`)
must be provided in the `job` section.
This includes `flexus_path`, `flexus_trace_path`, `flexus_timing_path`,
`user_postload_path`, `phases_name`, `phases_length`, `checkpoints_number`,
`checkpoints_length`, and `simulation_length`.

* **master_instance**: The name of the master instance,
selected from one of the instances listed under the `Instances` section
(described in `System`).
* **image_repo_path**: Base path to the images directory.
All image-related files *for all instances* must be in the same directory
for ease of deployment.