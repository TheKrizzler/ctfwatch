from colorama import Fore
import threading
import struct
import socket
import json
import time

CTFWATCH_AGENT_IP = "ctfwatch"
CTFWATCH_AGENT_PORT = 9494
DEBUG = False
MAX_EVENT_SIZE = 1024 * 1024 # 1 MiB max

ECS_FIELDS = {
    "timestamp": "@timestamp",
    "user_agent": "user_agent.original",
    "source_ip": "source.ip",
    "source_port": "source.port",
    "destination_ip": "destination.ip",
    "destination_port": "destination.port",
    "http_request_method": "http.request.method",
    "http_request_body": "http.request.body.content",
    "http_response_status_code": "http.response.status_code",
    "http_response_body": "http.response.body.content",
    "custom": "custom"
}

class Event():
    def __init__(self, **kwargs):
        self.timestamp = time.time()
        self.properties = kwargs
        self.properties.update({"timestamp": str(self.timestamp)})
        self.properties = log_to_ecs(self.properties)

    def serialize(self):
        return json.dumps(self.properties, separators=(",", ":")).encode("utf-8")

class Logger():
    def __init__(self, debug=False):
        global DEBUG
        DEBUG = debug
        self.sock = None
        self.lock = threading.Lock()

    def connect(self):
        if self.sock is None:
            self.sock = socket.create_connection(
                (CTFWATCH_AGENT_IP, CTFWATCH_AGENT_PORT),
                timeout=2
            )

    def send_event(self, event):
        data = event.serialize()

        if len(data) > MAX_EVENT_SIZE:
            info(f"Event too large, not transmitted! (length greater than {MAX_EVENT_SIZE} bytes)")
            return

        packed_data = struct.pack(">I", len(data)) + data # no, a single log entry cannot be longer than 2**32 bytes
        debug(f"Attempting to send packed data: {packed_data}")

        with self.lock:
            try:
                self.connect()
                self.sock.sendall(packed_data)
            except OSError:
                self.close()

                try:
                    self.connect()
                    self.sock.sendall(packed_data)
                except OSError as e:
                    self.close()
                    info(f"Failed to send event to agent: {e}")
                    return

            try:
                if self.sock.recv(2) != b'OK':
                    info("Received invalid agent response! Event was likely not received.")
                    self.close()
            except OSError as e:
                info(f"Did not receive server response: {e}")
                self.close()

    def close(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None

def info(msg):
    print(f"[{Fore.RED}ctfwatch{Fore.RESET}] {msg}")

def debug(msg):
    if DEBUG:
        info(f"[DEBUG] {msg}")

def log_to_ecs(properties: dict):
    ecs_output = {}

    for key,value in properties.items():
        if key in ECS_FIELDS:
            path = ECS_FIELDS[key].split('.')
        else:
            path = (key, )

        current = ecs_output
        for key in path[:-1]:
            current = current.setdefault(key, {})
        current[path[-1]] = value

    debug(f"ECS TRANSLATION : {ecs_output}")
    return ecs_output

logger = Logger()

def log(**kwargs):
    event = Event(**kwargs)
    logger.send_event(event)