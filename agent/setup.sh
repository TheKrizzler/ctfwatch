#!/usr/bin/env bash

set -e

NETWORK="ctfwatch-net"

if ! docker network inspect "$NETWORK" >/dev/null 2>&1; then
    echo "Creating Docker network: $NETWORK"
    docker network create "$NETWORK"
fi

echo "Starting CTFWatch agent..."
docker compose up -d --build

echo "CTFWatch agent started."