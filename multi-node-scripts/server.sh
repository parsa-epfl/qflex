#!/bin/bash


pushd ..

ln -s ./multi-node-scripts/server.expect  server.expect
expect server.expect

popd