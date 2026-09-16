# CTFWatch — Codex Project Context

## Project goal

CTFWatch is a lightweight centralized log collection and investigation system for local CTF infrastructure.

This is deliberately NOT an enterprise SIEM. Keep the architecture simple and minimize setup/integration work for challenge authors.

Primary goal:
- collect logs from CTF challenge containers
- enrich them with authoritative Docker/container metadata
- centrally store them
- provide simple browsing/search
- provide enough operational visibility to know whether logging is working

Optional detection/alerting may come later. Do not build a detection engine unless explicitly requested.

## Current architecture

Custom challenge
    |
    | Python ctfwatch.log(...)
    v
CTFWatch Agent (C)
    |
    | batched gzip HTTP
    v
CTFWatch Server (FastAPI)
    |
    v
PostgreSQL
    |
    +--> React log explorer
    |
    +--> Grafana eventually

The agent runs once per Docker host.

Challenge containers and the agent share an external Docker network:

    ctfwatch-net

The agent is reachable from challenge containers as:

    ctfwatch:9494

The server is independent of the challenge Docker network.

## Components

### Python logger

Challenge-author API should remain extremely simple:

    import ctfwatch

    ctfwatch.log(
        source_ip="8.8.8.8",
        source_port=1234,
        http_request_method="GET"
    )

Logger sends compact JSON over persistent TCP to the agent.

Protocol:

    [4-byte big-endian uint32 length][UTF-8 JSON]

Maximum event size:

    1 MiB

Zero-length frame is a ping/healthcheck.

The agent responds with:

    OK

Logger-controlled fields are untrusted.

## Trust model

Important architectural rule:

    logger  -> controls normal event fields
    agent   -> controls ctfwatch.container
    server  -> may eventually control ctfwatch.agent

A challenge MUST NOT be able to spoof its container identity.

When receiving logger events, the agent determines the originating container from the TCP peer IP and Docker metadata.

Any logger-supplied `ctfwatch` namespace should be removed/replaced by authoritative agent metadata.

Example:

    {
      "source": {
        "ip": "8.8.8.8"
      },
      "ctfwatch": {
        "container": {
          "id": "...",
          "name": "/challenge",
          "image": "...",
          "compose_project": "...",
          "compose_service": "..."
        }
      }
    }

## Agent

Written in C.

Important libraries:

- pthreads
- libcurl
- cJSON
- zlib
- vendored logging library

Agent mounts:

    /var/run/docker.sock

The agent already communicates with the Docker API through this Unix socket.

Container metadata structure currently resembles:

    typedef struct container_profile {
        struct sockaddr_storage ip_addr;
        socklen_t ip_addr_len;

        char *container_id;
        char *container_name;

        char *image_name;
        char *compose_project;
        char *compose_service;
    } container_profile_t;

Reuse existing Docker/container-profile code instead of implementing parallel Docker abstractions.

## Agent queue / forwarding

Workers produce cJSON events and push them into a shared FIFO queue.

A dedicated forwarder thread periodically detaches a batch.

Batch flush currently happens approximately:

- every 5 seconds
- or around 1 MiB batch size

Batch JSON:

    {
      "events": [
        {...},
        {...}
      ]
    }

Batch is gzip-compressed using zlib.

Then POSTed using libcurl.

Headers:

    Content-Type: application/json
    Content-Encoding: gzip
    Authorization: Bearer <agent token>

Agent config:

    /etc/ctfwatch/config.json

Example:

    {
      "server_url": "http://server/api/v1/logs/ingest",
      "token": "..."
    }

Do not create a second forwarding mechanism for new event sources.

Everything should ultimately feed the existing event queue.

## Current server

Python:

- FastAPI
- Uvicorn
- psycopg 3
- raw SQL
- PostgreSQL JSONB

No ORM.

Ingestion:

    POST /api/v1/logs/ingest

Server accepts gzip bodies.

Current database is approximately:

    CREATE TABLE events (
        id BIGSERIAL PRIMARY KEY,
        received_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        agent_id TEXT NOT NULL,
        data JSONB NOT NULL
    );

Indexes exist on:

- received_at
- JSONB data using GIN

Query endpoint:

    GET /api/v1/events

Current filters include:

- q
- container
- minutes
- limit
- offset

Free-text search currently uses:

    data::text ILIKE

This is intentionally simple.

## Frontend

React + Vite.

Built in a Node Docker build stage and copied into the FastAPI server image.

There is NOT a separate frontend runtime container.

Current UI provides:

- log search
- time-range selection
- container filtering
- event table
- expandable raw JSON

Do not replace this with Grafana.

Grafana may later be added for operational monitoring such as:

- events/sec
- events/min per container
- last received event
- database size
- disk usage
- identifying stopped log ingestion

React remains the actual log investigation interface.

## CURRENT TASK: generic Docker log collection

We now want CTFWatch to support arbitrary existing software such as:

- GitLab
- FTP servers
- nginx
- sshd
- arbitrary open-source challenge software

These applications cannot reasonably be expected to integrate the Python logger.

The solution is generic Docker stdout/stderr collection.

DO NOT attempt to understand every application's log format.

Preserve raw log messages.

Example generic event:

    {
      "message": "authentication failed for user admin",
      "log": {
        "stream": "stderr"
      },
      "ctfwatch": {
        "container": {
          "...": "authoritative metadata"
        }
      }
    }

These events go through the SAME queue, batching, server endpoint, database, and frontend as logger-generated events.

## Docker log collection opt-in

Log collection should be explicitly enabled per challenge using a Docker label.

Example:

    services:
      challenge:
        labels:
          ctfwatch.logs: "true"

Do not automatically collect stdout/stderr from every container.

## Required container discovery

Existing logger container identification is lazy:

    challenge connects to agent
            |
            v
    agent observes TCP peer IP
            |
            v
    Docker IP -> container lookup

Docker log collection cannot depend on a challenge initiating a connection.

Add proactive container discovery.

For the prototype, polling Docker approximately every 5 seconds is acceptable.

Conceptually:

    container monitor thread
            |
            v
    GET /containers/json
            |
            v
    find relevant running containers
            |
            v
    check ctfwatch.logs=true
            |
            v
    already followed?
       |             |
      yes            no
                     |
                     v
              spawn log reader

Prefer simple polling over Docker `/events` for now.

## Container monitor state

A simple linked-list/set of monitored container IDs is sufficient.

Something conceptually similar to:

    typedef struct monitored_container {
        char *container_id;
        pthread_t thread;
        struct monitored_container *next;
    } monitored_container_t;

    typedef struct container_monitor {
        event_queue_t *queue;
        monitored_container_t *containers;
        pthread_mutex_t lock;
    } container_monitor_t;

Exact types should fit the existing codebase rather than blindly copying this.

Avoid duplicate log-reader threads for the same container.

When a container's log stream closes, its reader exits and the container should eventually become eligible for monitoring again if restarted/recreated.

Keep lifecycle handling simple.

## Docker logs API

For each opted-in container, follow:

    GET /containers/<id>/logs?stdout=1&stderr=1&follow=1&timestamps=1

Use the existing libcurl dependency and Docker Unix socket:

    /var/run/docker.sock

Conceptually:

    curl_easy_setopt(
        curl,
        CURLOPT_UNIX_SOCKET_PATH,
        "/var/run/docker.sock"
    );

The request is streaming.

Do NOT assume one libcurl write callback equals one log record.

## Docker stream framing

For non-TTY containers Docker normally multiplexes stdout/stderr.

Each record has an 8-byte header:

    byte 0      stream
                1 = stdout
                2 = stderr

    bytes 1-3   unused

    bytes 4-7   payload length
                unsigned 32-bit big endian

    then N payload bytes

libcurl may split data arbitrarily:

    callback 1: half header
    callback 2: rest of header + part payload
    callback 3: rest payload + next header

Therefore maintain an accumulation buffer / parser state.

Do not parse each curl callback as a complete message.

A small abstraction such as:

    void log_stream_feed(
        log_stream_t *stream,
        const unsigned char *data,
        size_t length
    );

is appropriate if it fits the existing code.

When a complete record is obtained:

1. determine stdout/stderr
2. preserve the raw message
3. create a cJSON event
4. attach authoritative container metadata
5. push it into the existing event queue

Do not POST directly from the reader thread.

## TTY caveat

Docker containers configured with TTY may not use the stdout/stderr multiplex framing above.

Keep this in mind when implementing the parser.

Do not massively complicate the prototype solely for unusual TTY configurations, but do not blindly interpret raw TTY output as multiplex headers.

Inspect Docker container metadata if necessary.

## Event semantics

Generic Docker logs should remain generic.

At minimum:

    {
      "message": "...",
      "log": {
        "stream": "stdout"
      },
      "ctfwatch": {
        "container": {...}
      }
    }

Do not implement application-specific parsers unless explicitly requested.

Do not try to recognize GitLab/nginx/FTP/etc.

Raw searchable logs are the feature.

## Network packet monitoring

We discussed packet sniffing but it is NOT the current implementation direction.

Do not build a packet sniffer.

The preferred hierarchy is:

    1. ctfwatch.log()
       structured application events for custom challenges

    2. Docker stdout/stderr collection
       generic events for arbitrary software

    3. optional network sensor
       possible future supplemental feature

## Setup

Agent setup script should remain simple.

Current intended behavior:

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

Agent Compose declares:

    networks:
      ctfwatch-net:
        external: true

## Development priorities

This is a prototype under time pressure.

Prioritize:

1. working implementation
2. simple ownership
3. readable C
4. reuse existing infrastructure
5. minimal challenge-author setup

Avoid introducing:

- Redis
- Kafka
- OpenSearch
- Elasticsearch
- ORM
- complicated async frameworks
- unnecessary services
- protocol-specific log parsers
- elaborate abstraction layers

A few straightforward C files/functions are preferable to an enterprise architecture.

## Known non-blocking agent TODOs

Do not derail the current task to fix these unless directly necessary:

- failed HTTP batches currently may be lost
- queue should eventually have a hard cap
- graceful shutdown
- exact send helper
- monotonic timers
- stronger agent authentication
- detection engine

## Coding approach

Before changing code:

1. inspect the existing repository
2. identify existing Docker API helpers
3. identify container profile ownership/destruction
4. identify queue ownership semantics
5. identify cJSON ownership semantics
6. reuse those mechanisms

Do not assume the example structures in this document exactly match the current source.

Prefer incremental modifications over rewriting the agent.

For the current Docker-log task, first explain the proposed changes based on the ACTUAL existing source tree, then implement them.
