#!/bin/sh

set -e

cd "$(dirname "$0")/.."

sed -i "s|path:.*[\\/]|path: $(pwd)/components/|g" dependencies.lock

cat dependencies.lock

idf.py build
