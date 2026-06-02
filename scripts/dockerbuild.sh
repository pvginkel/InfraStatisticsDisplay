#!/bin/sh

set -e

cd "$(dirname "$0")/.."

idf.py build
