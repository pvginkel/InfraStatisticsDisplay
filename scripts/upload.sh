#!/bin/sh

set -e

cd "$(dirname "$0")/.."

HOST=${1:-http://iotsupport.iotsupport.svc.cluster.local}

curl -fsSL $HOST/api/pipeline/upload.sh | sh -s --
