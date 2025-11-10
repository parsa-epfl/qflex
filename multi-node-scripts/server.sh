#!/bin/bash


pushd ..

ln -s ./multi-node-scripts/server.exp  server.exp
expect server.exp

popd