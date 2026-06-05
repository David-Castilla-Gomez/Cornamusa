#!/usr/bin/env python3
"""
Smoke del LSP completion (v1.127).

Lanza el LSP, hace initialize, didOpen y un completion request sobre un
documento minimo con una funcion + una clase top-level. Verifica:
  - initialize anuncia completionProvider en capabilities.
  - completion devuelve una lista no-vacia con al menos 100 items
    (las nativas son ~120).
  - Las labels incluyen nativas conocidas (imprimir, longitud),
    keywords (si, funcion), y los simbolos top-level (saludar, Persona).
"""
import subprocess, json, sys


def send(proc, body):
    s = json.dumps(body)
    msg = f"Content-Length: {len(s)}\r\n\r\n{s}"
    proc.stdin.write(msg.encode())
    proc.stdin.flush()


def read_response(proc):
    headers = b""
    while b"\r\n\r\n" not in headers:
        chunk = proc.stdout.read(1)
        if not chunk:
            return None
        headers += chunk
    cl = 0
    for line in headers.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            cl = int(line.split(b":")[1].strip())
    body = b""
    while len(body) < cl:
        chunk = proc.stdout.read(cl - len(body))
        if not chunk:
            return None
        body += chunk
    return json.loads(body)


def read_resp_with_id(proc, want_id, max_msgs=30):
    for _ in range(max_msgs):
        r = read_response(proc)
        if r and r.get("id") == want_id:
            return r
    return None


def main():
    if len(sys.argv) < 2:
        print("USO: lsp_completion.py <ruta_a_cornamusa>", file=sys.stderr)
        sys.exit(2)
    exe = sys.argv[1]

    proc = subprocess.Popen([exe, "lsp"], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # 1. initialize
    send(proc, {"jsonrpc": "2.0", "id": 1, "method": "initialize",
                "params": {}})
    init_resp = read_resp_with_id(proc, 1)
    assert init_resp is not None, "no initialize response"
    caps = init_resp["result"]["capabilities"]
    assert "completionProvider" in caps, "completionProvider missing"

    # 2. didOpen con un .cor de prueba
    send(proc, {"jsonrpc": "2.0", "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": "file:///test.cor",
                        "languageId": "cornamusa",
                        "version": 1,
                        "text": ("funcion saludar():\n"
                                 "    imprimir(\"hola\")\n"
                                 "fin funcion\n"
                                 "\n"
                                 "clase Persona:\n"
                                 "    pasar\n"
                                 "fin clase\n")
                    }}})

    # 3. completion
    send(proc, {"jsonrpc": "2.0", "id": 2,
                "method": "textDocument/completion",
                "params": {
                    "textDocument": {"uri": "file:///test.cor"},
                    "position": {"line": 0, "character": 0}}})
    comp_resp = read_resp_with_id(proc, 2)
    assert comp_resp is not None, "no completion response"
    items = comp_resp["result"]["items"]
    assert len(items) > 100, f"pocos items: {len(items)}"
    labels = {it["label"] for it in items}
    for expected in ("imprimir", "longitud", "funcion", "si",
                     "saludar", "Persona"):
        assert expected in labels, f"falta label {expected!r}"

    # 4. shutdown + exit
    send(proc, {"jsonrpc": "2.0", "id": 3, "method": "shutdown",
                "params": {}})
    read_resp_with_id(proc, 3)
    send(proc, {"jsonrpc": "2.0", "method": "exit", "params": {}})
    rc = proc.wait(timeout=5)
    assert rc == 0, f"exit no-cero: {rc}"
    print("OK: lsp_completion")


if __name__ == "__main__":
    main()
