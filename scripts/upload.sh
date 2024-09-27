#!/bin/sh

set -e

TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
SIGNATURE=$(echo -n $TIMESTAMP | openssl dgst -sha256 -sign "$1" | base64)

curl \
    --output - \
    -F "file=@$2" \
    -F "timestamp=$TIMESTAMP" \
    -F "signature=$SIGNATURE" \
    http://iotsupport.iotsupport.svc.cluster.local/assetctl/upload.php
