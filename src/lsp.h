#ifndef CORNAMUSA_LSP_H
#define CORNAMUSA_LSP_H

/*
 * Servidor LSP (Language Server Protocol) MVP para Cornamusa (v1.52).
 *
 * Implementa el subset minimo para diagnostics en tiempo real:
 *   - initialize / initialized / shutdown / exit
 *   - textDocument/didOpen / didChange / didClose
 *   - textDocument/publishDiagnostics (notification al editor)
 *
 * NO implementa (queda para v1.53+):
 *   - textDocument/hover (necesitaria mapear posiciones a nodos AST)
 *   - textDocument/definition, completion, codeAction
 *   - workspace/* requests
 *   - Incremental document sync (usamos sincronizacion completa)
 *
 * El servidor se comunica via stdin/stdout con framing JSON-RPC
 * (Content-Length: N\r\n\r\n + body). En Windows stdin/stdout DEBEN
 * estar en modo binario; el llamador (main.c) lo configura antes
 * de invocar `lsp_run`.
 */

/* Ejecuta el bucle del servidor LSP hasta que el cliente envia
 * `exit`. Devuelve 0 en exit limpio. */
int lsp_run(void);

#endif /* CORNAMUSA_LSP_H */
