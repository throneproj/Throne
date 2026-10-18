#!/usr/bin/env python3
"""Loopback fixtures for script/ci/headless_smoke.sh: HTTP 204, and a SOCKS5 proxy that logs every CONNECT.

SOCKS5 resolves HOSTS itself; the HTTP delay keeps a loopback latency test above 0 ms, which Throne reads as untested.
"""

import argparse
import http.server
import os
import select
import signal
import socket
import socketserver
import struct
import sys
import threading
import time

HOSTS = {"fixture.test": "127.0.0.1"}


class EventLog:
    def __init__(self, path):
        self._file = open(path, "a", encoding="utf-8")
        self._lock = threading.Lock()

    def line(self, text):
        with self._lock:
            self._file.write(text + "\n")
            self._file.flush()


class NoContentHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    server_version = "ThroneSmoke/1"

    def _no_content(self):
        time.sleep(self.server.delay)
        self.send_response(204)
        self.end_headers()
        self.server.log.line(f"HTTP {self.command} {self.headers.get('Host', '-')}{self.path} 204")

    do_GET = _no_content
    do_HEAD = _no_content

    def log_message(self, format, *args):
        pass


class NoContentServer(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, log, delay):
        self.log = log
        self.delay = delay
        super().__init__(address, NoContentHandler)


def recv_exact(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("the client closed the connection mid-request")
        data += chunk
    return data


def socks_reply(sock, code, bound=("0.0.0.0", 0)):
    host, port = bound[0], bound[1]
    if ":" in host:
        address = b"\x04" + socket.inet_pton(socket.AF_INET6, host)
    else:
        address = b"\x01" + socket.inet_pton(socket.AF_INET, host)
    sock.sendall(b"\x05" + bytes([code, 0]) + address + struct.pack("!H", port))


def relay(client, upstream):
    peers = {client: upstream, upstream: client}
    open_ends = {client, upstream}
    for sock in peers:
        sock.settimeout(None)
    while open_ends:
        readable, _, _ = select.select(list(open_ends), [], [], 120)
        if not readable:
            return
        for sock in readable:
            try:
                data = sock.recv(65536)
            except OSError:
                data = b""
            if not data:
                open_ends.discard(sock)
                try:
                    peers[sock].shutdown(socket.SHUT_WR)
                except OSError:
                    pass
                continue
            try:
                peers[sock].sendall(data)
            except OSError:
                return


class SocksHandler(socketserver.BaseRequestHandler):
    def handle(self):
        client = self.request
        client.settimeout(15)
        log = self.server.log
        try:
            version, count = recv_exact(client, 2)
            if version != 5:
                log.line(f"SOCKS version {version} refused")
                return
            if 0 not in recv_exact(client, count):
                client.sendall(b"\x05\xff")
                log.line("SOCKS refused: the client offers no 'no authentication' method")
                return
            client.sendall(b"\x05\x00")
            _, command, _, kind = recv_exact(client, 4)
            if kind == 1:
                host = socket.inet_ntop(socket.AF_INET, recv_exact(client, 4))
            elif kind == 3:
                host = recv_exact(client, recv_exact(client, 1)[0]).decode("ascii", "replace")
            elif kind == 4:
                host = socket.inet_ntop(socket.AF_INET6, recv_exact(client, 16))
            else:
                socks_reply(client, 8)
                log.line(f"SOCKS refused: address type {kind}")
                return
            (port,) = struct.unpack("!H", recv_exact(client, 2))
            target = f"[{host}]:{port}" if ":" in host else f"{host}:{port}"
            if command != 1:
                socks_reply(client, 7)
                log.line(f"COMMAND {command} {target} refused: only CONNECT is supported")
                return
            address = HOSTS.get(host.lower().rstrip("."), host)
            via = f"[{address}]:{port}" if ":" in address else f"{address}:{port}"
            try:
                upstream = socket.create_connection((address, port), timeout=10)
            except OSError as error:
                socks_reply(client, 5)
                log.line(f"CONNECT {target} -> {via} failed: {error}")
                return
            with upstream:
                socks_reply(client, 0, upstream.getsockname()[:2])
                log.line(f"CONNECT {target} -> {via} ok")
                relay(client, upstream)
        except (OSError, ValueError) as error:
            log.line(f"SOCKS error: {error}")


class SocksServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address, log):
        self.log = log
        super().__init__(address, SocksHandler)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--listen", default="127.0.0.1", help="address both servers bind (default 127.0.0.1)")
    parser.add_argument("--http-port", type=int, default=18080)
    parser.add_argument("--socks-port", type=int, default=1080)
    parser.add_argument("--http-delay-ms", type=int, default=25, help="wait before each 204 (default 25)")
    parser.add_argument("--log", required=True, help="file that gets one line per HTTP request and SOCKS request")
    parser.add_argument("--ready-file", help="written once both servers listen")
    parser.add_argument("--alias", action="append", default=[], metavar="ADDR",
                        help="a SOCKS5 CONNECT to ADDR (a name or an address) reaches this HTTP server; repeatable")
    args = parser.parse_args()
    for alias in args.alias:
        HOSTS[alias.lower().rstrip(".")] = "127.0.0.1"

    log = EventLog(args.log)
    try:
        servers = [
            NoContentServer((args.listen, args.http_port), log, args.http_delay_ms / 1000),
            SocksServer((args.listen, args.socks_port), log),
        ]
    except OSError as error:
        print(f"smoke_fixtures: cannot listen: {error}", file=sys.stderr)
        return 1
    for server in servers:
        threading.Thread(target=server.serve_forever, daemon=True).start()

    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(0))
    print(f"smoke_fixtures: HTTP 204 on {args.listen}:{args.http_port}, SOCKS5 on {args.listen}:{args.socks_port}",
          file=sys.stderr, flush=True)
    if args.ready_file:
        partial = args.ready_file + ".partial"
        with open(partial, "w", encoding="utf-8") as ready:
            ready.write(f"{os.getpid()}\n")
        os.replace(partial, args.ready_file)
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
