#!/bin/bash


pushd ..

ln -s ./multi-node-scripts/client.exp  client.exp
expect client.exp

popd