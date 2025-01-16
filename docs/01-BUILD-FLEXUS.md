BUILD FLEXUS
============

Installing QFlex require building both QEMU and Flexus (the uArch simulators).
This part focus on building __Flexus__

## Docker
It is the easiest way of building __Flexus__
You can both build docker and use it to run qflex through docker.

```bash
./build docker {debug}
```

The debug flag can be empty or set as debug to have the image be built in debug mode.

## Flexus

Before anything make sure you have the

Flexus buildsystem has tow main parts.
First, the dependencies are managed by [Conan](https://docs.conan.io/2.0/index.html). This will download (and/or build) the required depedencies to build
Flexus. [Have look at this page to install conan](https://docs.conan.io/2.0/installation.html)

All the informations for this part is contained in the `conanfile.py`.
Secondly, the compilations options and linking options are handled by CMake, and the
description is contained in the `CMakeList.txt` file.

Before everyting conan needs to know what are the compiler and standard the machine
used for building is capabale of. Therefore you need to run once for all futur build

```bash
conan profile detect
```

### Build Flexus - User mode
This is aimed toward people who mainly need to use QFlex.

Then call the build script using the one of the following options.
```bash
./build [keenkraken/knottykraken]
```

This will produce a directory called `out` in your current working directory.

#### Building `release` or `debug` version
Both QEMU and Flexus can be build both for production or developement.
The developement version enable the maximum debug compilation for GCC, and
add the address sanitizer (ASAN) with the flag `-fsanitize=address`, while also intoducing the frame pointer with `-fno-omit-frame-pointer`.

To do so, use the debug flag (release|debug) when appropriate.

```bash
./build [receipt] [(release|debug)]
```

### Build Flexus - Developer mode
For those who will frequently build any of the Flexus simulators, here is the description
of the CLI input to have the maximum understanding of the build process.

#### Understanding the profile
[Profiles Doc](https://docs.conan.io/2.0/reference/config_files/profiles.html)
The profile in flexus `target/_profile` directory are used to
modified the behaviour of the dependency, compiler version, and standard version, or the build type
in this case wether `Release`or `Debug`. These also have an influence on the verbosity of the output while building.

#### Installing the dependencies
The command install the dependencies for a given simulator and a given profile

The name is required otherwise nothing will be built. The profile are recommended otherwise
the output might not be conform nor buildable.
```bash
conan install [dir with conanfile.py] -pr [profile] --name=[keenkraken/knottykraken] -of [output dir] -b missing
```

#### Building Flexus
Building will also install the depdencies if they are not installed anyway.
After installing the dependencies if any, this command will trigger CMake which will actually compile Flexus source code.
```bash
conan build [dir with conanfile.py] -pr [profile] --name=[keenkraken/knottykraken] -of [output dir] -b missing
```

#### Exporting FLexus
Used as a commodity, the following commands copy the output of the previous build to a more convinient location.
As for now it is the top most location in the output directory.
This will be hardly used by anyone developping because this require calling this command after every build.
```bash
conan export-pkg [dir with conanfile.py] -pr [profile] --name=[keenkraken/knottykraken] -of [output dir]
```

### Flexus Build - Expert mode
Ask Bryan !!
