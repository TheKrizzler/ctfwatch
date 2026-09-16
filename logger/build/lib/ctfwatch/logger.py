from colorama import Fore
import threading
import struct
import socket
import json
import time
import os

DEBUG = False

HOST = os.getenv("CTFWATCH_HOST", "ctfwatch")
PORT = int(os.getenv("CTFWATCH_PORT", "9494"))
MAX_EVENT_SIZE = 1024 * 1024 # 1 MiB max

ECS_FIELDS = {
    # Event
    "timestamp": "@timestamp",
    "message": "message",
    "event_action": "event.action",
    "event_category": "event.category",
    "event_kind": "event.kind",
    "event_type": "event.type",
    "event_outcome": "event.outcome",
    "event_reason": "event.reason",
    "event_duration": "event.duration",

    # User
    "user_id": "user.id",
    "user_name": "user.name",
    "user_email": "user.email",
    "user_domain": "user.domain",

    # User agent
    "user_agent": "user_agent.original",

    # Source
    "source_ip": "source.ip",
    "source_port": "source.port",
    "source_address": "source.address",
    "source_domain": "source.domain",
    "source_mac": "source.mac",
    "source_bytes": "source.bytes",

    # Destination
    "destination_ip": "destination.ip",
    "destination_port": "destination.port",
    "destination_address": "destination.address",
    "destination_domain": "destination.domain",
    "destination_mac": "destination.mac",
    "destination_bytes": "destination.bytes",

    # Client
    "client_ip": "client.ip",
    "client_port": "client.port",
    "client_address": "client.address",
    "client_domain": "client.domain",

    # Server
    "server_ip": "server.ip",
    "server_port": "server.port",
    "server_address": "server.address",
    "server_domain": "server.domain",

    # Network
    "network_protocol": "network.protocol",
    "network_transport": "network.transport",
    "network_direction": "network.direction",
    "network_bytes": "network.bytes",
    "network_packets": "network.packets",

    # HTTP request
    "http_request_method": "http.request.method",
    "http_request_body": "http.request.body.content",
    "http_request_bytes": "http.request.bytes",
    "http_request_referrer": "http.request.referrer",

    # HTTP response
    "http_response_status_code": "http.response.status_code",
    "http_response_body": "http.response.body.content",
    "http_response_bytes": "http.response.bytes",

    # URL
    "url_original": "url.original",
    "url_full": "url.full",
    "url_scheme": "url.scheme",
    "url_domain": "url.domain",
    "url_port": "url.port",
    "url_path": "url.path",
    "url_query": "url.query",
    "url_fragment": "url.fragment",

    # DNS
    "dns_question_name": "dns.question.name",
    "dns_question_type": "dns.question.type",
    "dns_response_code": "dns.response_code",
    "dns_resolved_ip": "dns.resolved_ip",

    # TLS
    "tls_version": "tls.version",
    "tls_cipher": "tls.cipher",
    "tls_established": "tls.established",

    # Process
    "process_pid": "process.pid",
    "process_name": "process.name",
    "process_executable": "process.executable",
    "process_command_line": "process.command_line",
    "process_parent_pid": "process.parent.pid",
    "process_parent_name": "process.parent.name",

    # File
    "file_name": "file.name",
    "file_path": "file.path",
    "file_extension": "file.extension",
    "file_size": "file.size",
    "file_hash_md5": "file.hash.md5",
    "file_hash_sha1": "file.hash.sha1",
    "file_hash_sha256": "file.hash.sha256",

    # Error
    "error_code": "error.code",
    "error_message": "error.message",
    "error_type": "error.type",
    "error_stack_trace": "error.stack_trace",

    # Host
    "host_name": "host.name",
    "host_hostname": "host.hostname",
    "host_ip": "host.ip",
    "host_mac": "host.mac",
    "host_os_name": "host.os.name",

    # Service
    "service_name": "service.name",
    "service_type": "service.type",
    "service_version": "service.version",

    # Custom CTFWatch data
    "custom": "custom",
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
                timeout=10
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