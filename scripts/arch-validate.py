#!/usr/bin/env python3
#
# arch-validate — submit architecture artifact(s) to the validation service.
#
# Usage:
#   arch-validate <path> [<path>...]    validate one or more YAML files
#   arch-validate -                     read a single artifact from stdin
#
# Flags:
#   --json                emit the raw endpoint response to stdout
#   --quiet               suppress per-file "OK" lines; only print on failure
#
# Environment:
#   ARCHITECTURE_VALIDATE_URL    override the endpoint (default: production service)
#
# Exit codes: 0 valid, 1 invalid, 2 transport/server error.
#
# Runs on python:slim — uses only the standard library.

import argparse
import json
import os
import sys
import urllib.error
import urllib.request

DEFAULT_ENDPOINT = "https://architecture.webathome.org/api/validate"


def parse_args():
    p = argparse.ArgumentParser(
        prog="arch-validate",
        description="Submit architecture artifact(s) to the validation service.",
        add_help=True,
    )
    p.add_argument("--json", dest="json_mode", action="store_true",
                   help="emit the raw endpoint response to stdout")
    p.add_argument("--quiet", action="store_true",
                   help='suppress per-file "OK" lines; only print on failure')
    p.add_argument("paths", nargs="+",
                   help="artifact paths, or '-' to read one artifact from stdin")
    return p.parse_args()


def colors():
    if sys.stderr.isatty():
        return {"red": "\033[31m", "green": "\033[32m",
                "dim": "\033[2m", "reset": "\033[0m"}
    return {"red": "", "green": "", "dim": "", "reset": ""}


def read_body(path):
    if path == "-":
        return sys.stdin.read()
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def display_name(path):
    return "<stdin>" if path == "-" else path


def post(endpoint, body):
    """Return (http_code, response_text). Raises urllib.error.URLError on transport failure."""
    req = urllib.request.Request(
        endpoint,
        data=body.encode("utf-8"),
        headers={"Content-Type": "application/yaml"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")


def print_human(name, response, quiet, c):
    try:
        data = json.loads(response)
    except json.JSONDecodeError:
        sys.stderr.write(f"{c['red']}✗{c['reset']} {name}\n")
        sys.stderr.write("  server returned non-JSON body:\n")
        for line in response.splitlines():
            sys.stderr.write(f"    {line}\n")
        return False

    if data.get("valid") is True:
        if not quiet:
            sys.stderr.write(f"{c['green']}✓{c['reset']} {name}\n")
        return True

    sys.stderr.write(f"{c['red']}✗{c['reset']} {name}\n")
    errors = data.get("errors") or []
    if not errors:
        msg = data.get("error", "(unknown failure)")
        sys.stderr.write(f"  {msg}\n")
        return False

    for err in errors:
        sys.stderr.write(f"  {err.get('path', '')}\n")
        sys.stderr.write(f"    {err.get('message', '')}\n")
        sys.stderr.write(f"    schema: {err.get('schemaUrl', '')}\n")
        if err.get("hint"):
            sys.stderr.write(f"    hint: {err['hint']}\n")
    return False


def print_server_error(name, http_code, response, c):
    sys.stderr.write(f"{c['red']}✗{c['reset']} {name} {c['dim']}(HTTP {http_code}){c['reset']}\n")
    if not response:
        sys.stderr.write("  server returned no body\n")
        return
    try:
        data = json.loads(response)
        msg = data.get("error") if isinstance(data, dict) else None
        sys.stderr.write(f"  {msg if msg else json.dumps(data)}\n")
    except json.JSONDecodeError:
        for line in response.splitlines():
            sys.stderr.write(f"  {line}\n")


def main():
    args = parse_args()
    endpoint = os.environ.get("ARCHITECTURE_VALIDATE_URL", DEFAULT_ENDPOINT)
    c = colors()
    status = 0

    for artifact in args.paths:
        name = display_name(artifact)

        try:
            body = read_body(artifact)
        except OSError as e:
            sys.stderr.write(f"{c['red']}✗{c['reset']} {name}\n")
            sys.stderr.write(f"  cannot read file: {e}\n")
            status = 2
            continue

        try:
            http_code, response = post(endpoint, body)
        except urllib.error.URLError as e:
            sys.stderr.write(f"{c['red']}✗{c['reset']} {name}\n")
            sys.stderr.write(f"  transport error: could not reach {endpoint}\n")
            sys.stderr.write(f"    {e.reason}\n")
            status = 2
            continue

        if 200 <= http_code < 300:
            if args.json_mode:
                sys.stdout.write(response if response.endswith("\n") else response + "\n")
                try:
                    if json.loads(response).get("valid") is not True and status < 1:
                        status = 1
                except json.JSONDecodeError:
                    if status < 1:
                        status = 1
            else:
                ok = print_human(name, response, args.quiet, c)
                if not ok and status < 1:
                    status = 1
            continue

        # 4xx: server rejected the client's request (treat as invalid).
        # 5xx: server-side failure (transport-class).
        print_server_error(name, http_code, response, c)
        if 400 <= http_code < 500:
            if status < 1:
                status = 1
        else:
            status = 2

    sys.exit(status)


if __name__ == "__main__":
    main()
