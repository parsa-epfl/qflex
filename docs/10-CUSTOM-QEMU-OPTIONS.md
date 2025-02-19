Custom QEMU Options
===================

This documents presents the custom options that were added to QEMU which add new features.

## libqflex
The flag `-libqflex` contain 6 arguments/options.

* `mode`: defaulted to `timing`. This setup QEMU for KnottyKraken. This is kept for compatibility, because it used to trigger a Trace mode, which has been deleted.
* `lib-path`: path of the `.so` to load. This should point to the right shared object based on the type of simulation.
* `cfg-path`: path of the file that will be used to configure the simulation.
* `ckpt-path`: path to the folder that will be used to load the micro-architectural state of KnottyKraken
* `cycles`: splited in 3 parts `[max cycle]:[stat dump interval]:[log dump delay]`. `[max cycle]` is the maximum number of cycles to simulate before KnottyKraken quit. `[stat dump interval]` sets the interval of cycles between each dump. `[log dump delay]` prevents any log output until the simulation reach the number of cycles set. Only `[max cycle]` is needed.
* `freq`: @see [21-FLEXUS-SPLIT-FREQUENCY.md](./21-FLEXUS-SPLIT-FREQUENCY.md)
* `debug`: the type of log output, default is `vverb`.
    * `crit`: Only critical issue, mostly nothing, speed up simulation a lot.
    * `dev`: Information about core switching, and instruction retiremments.
    * `verb`: Log all cache lookup and validation
    * `vverb`: All possible inforamtion, about each cycles, slow down simulation a lot.

## snapvm-external
This feature is composed of 2 top level options: `snapvm-external` and `loadvm-external`.

`-snapvm-external` must have a `path` arguments that point to a folder (existing or not), where the external snapshot will be dumped.

`-loadvm-external` will always superseed the regular `loadvm` and there for should not be used togehter. This must have a nameless arugments. The arguments must be a filename located at `-snapvm-external path=`, likely with the `.zstd` file extension.

```
snapvm-external
    path            ${ROOT}/Busybox/external

loadvm-external
    vm-2024_11_03-2034_52.zstd
```


In the previous exemple, `${ROOT}/Busybox/external/vm-2024_11_03-2034_52.zstd` is a *readable* and *valid* zstd compressed file.

> [!WARNING]
> Technically speaking, `loadvm-external` can also read and load raw QEMU state file, this feature is however not well documented, because it resorts on the ZSTD tools passing through non-zstd compressed file. Use with CAUTION.

