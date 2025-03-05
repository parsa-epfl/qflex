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

```txt
snapshot_4.zstd
snapshot_3.zstd
snapshot_2.zstd
snapshot_1.zstd
snapshot_0.zstd
```

In the upper exemple, `snapshot_0.zstd` is the base layer, also the first snapshot made. Opposed to `snapshot_4.zstd`, the last snapshot made in time.


To generate this kind of file, the following command line is suggested.

```bash
ls -t snapshot_*.zstd > increments.txt
```
> [!WARNING]
> Be sure to check the generated file afterward because the `-t` flag sort file by time of creation, cf. `-t     sort by time, newest first; see --time`

### Compression
Both procedures can expect to read ZSTD compress QEMU' state file. This means that the file must be decompressed before creating a file, or before being used to recreate a file. This option can be enabled using `--zstd`.

## Encode

The encoding creates a `.patch` file that has the same name as the file it will recreate if applied to the previous snapshot.

## Merge

`xdelta3` has a merge feature, which allows to merge multiple patch into one single file. This can be usefull if you need to decode `snapshot_21` without actullny needing to decode any of the previous snapshots. The command is better explained if you access `xdelta -h`, but here is a sample to merge 3 patch togheter.

```bash
# Notice that the last patch nor the output patch do need the flag `-m`
xdelta3 merge -m snapshot_1.patch -m snapshot_2.patch snapshot_3.patch merged.patch
```


