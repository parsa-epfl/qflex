#!/usr/bin/env bash
#
# _           _        _ _    __ _
#(_)_ __  ___| |_ __ _| | |  / _| | _____  ___   _ ___
#| | '_ \/ __| __/ _` | | | | |_| |/ _ \ \/ / | | / __|
#| | | | \__ \ || (_| | | | |  _| |  __/>  <| |_| \__ \
#|_|_| |_|___/\__\__,_|_|_| |_| |_|\___/_/\_\\__,_|___/
#
#   Bryan Perdrizat
#       This script will install and build all differents version on Flexus.
#       It is functional, but should be mainly kept as a documentation
#       regarding how conan operate.

set -xev

output=${1:-./build}
flexus=$(realpath flexus/target)
# Build dependencies
conan install $flexus/keenkraken -pr $flexus/_profile/debug -b missing -of $output
conan install $flexus/keenkraken -pr $flexus/_profile/release -b missing -of $output
conan install $flexus/knottykraken -pr $flexus/_profile/debug -b missing -of $output
conan install $flexus/knottykraken -pr $flexus/_profile/release -b missing -of $output
# Build Flexus
conan build $flexus/keenkraken -pr $flexus/_profile/debug -b missing -of $output
conan build $flexus/keenkraken -pr $flexus/_profile/release -b missing -of $output
conan build $flexus/knottykraken -pr $flexus/_profile/debug -b missing -of $output
conan build $flexus/knottykraken -pr $flexus/_profile/release -b missing -of $output
# Export the .so
conan export-pkg $flexus/keenkraken -pr $flexus/_profile/debug -of $output
conan export-pkg $flexus/keenkraken -pr $flexus/_profile/release -of $output
conan export-pkg $flexus/knottykraken -pr $flexus/_profile/debug -of $output
conan export-pkg $flexus/knottykraken -pr $flexus/_profile/release -of $output