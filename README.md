# ctfwatch

A logging pipeline and AI detection system designed for seamless integration with containerized challenges.

The project consists of 3 central parts: the central server, the agent, and a python logging wrapper.

## Server

This central component is a containerized Python Flask server which handles forwarded logs from the agents. These logs are already parsed and delivered as ECS. The server only accepts incoming API requests which are authenticated with a token only agents know.

Setup:
```sh
$ cd ./server
$ ./setup.sh
```

## Agent

One agent runs on each host and retrieves logs from all logging challenges. Communication from server to agent goes over TCP through the docker network 'ctfwatch' on port 9494. 

Setup:
```sh
$ cd ./agent
$ ./setup.sh
```

## Logger

The logger component is a lightweight Python library which seamlessly integrates with most Python scripts. Allows for registering events without the need for unreliable text parsing.

Setup:
```sh
$ pip install ./logger
```

The following example shows example usage to log a single event:

```py
import ctfwatch

ctfwatch.log(
    source_ip="8.8.8.8",
    user_agent="Claude/Chrome(1.2.2) ...",
    http_request_method="POST",
    http_request_body="{\"username\":\"admin\", \"password\":\"123\"}",
    http_response_status_code="HTTP/2 200 OK",
    http_response_body="{\"result\":\"success\"}",
    custom={"flag_printed": True}
)
```

