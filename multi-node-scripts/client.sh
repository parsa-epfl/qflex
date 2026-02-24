#!/bin/bash


pushd ..

ln -s ./multi-node-scripts/client.expect  client.expect
expect client.expect

popd