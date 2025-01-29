INCREMENTAL SNAPSHOTS
=====================


Incremental snapshots have the purpose of avoiding wasting space on the disk by making repeated snapshots of a workload. Traditional snapshots using QEMU's `savevm` commands create full machine state snapshots, including memory from all drivers as well as the RAM. However, when a workload fully loads the RAM, this one is saved in its entirety in a snapshot. Therefore, repeated snapshots of warmed-up workloads might waste a lot of space on the disk.

The incremental snapshot feature aims to address this issue by creating a binary patch between snapshot `n` and snapshot `n+1`. To create a patch, two steps are required :
    1. Generate QEMU's state file
    2. Use `xdelta3` to create a patch between two state files


The `bash` script in the folder `scripts/increments_snapshots/` aims to automate the point #2.


## Encode/Decode

Both procedures were scripted in their respective files. They both expect as first arguments a relative or absolute file path.

### Snapshot layers' file
The file given as an argument *MUST* contain:
* One relative path to a snapshot file per line

In the following order:
* The top most, respectively, first read file is the latest snapshot.
* Bottom most, respectively, the last read file is the base layers.

### Compression
Both procedures can expect to read ZSTD compress QEMU' state file. This means that the file must be decompressed before creating a file, or before being used to recreate a file. This option can be enabled using `--zstd`.

## Encode

The encoding creates a `.patch` file that has the same name as the file it will recreate if applied to the previous snapshot.
