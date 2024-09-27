#!/bin/sh

cd "$(dirname "$0")/.."

git clean -xdff
git submodule foreach --recursive 'git clean -xdff'
