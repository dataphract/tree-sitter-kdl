#include <limits.h>
#include <stdbool.h>
#include <wctype.h>

#include "tree_sitter/parser.h"

// The order of the enum values must match the order in the grammar.js
// `externals` array. The names themselves do not matter.
enum TokenType {
  MULTI_LINE_COMMENT,
  RAW_STRING,
};

void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

void *tree_sitter_kdl_external_scanner_create() { return NULL; }
void tree_sitter_kdl_external_scanner_reset(void *p) {}
void tree_sitter_kdl_external_scanner_destroy(void *p) {}

unsigned tree_sitter_kdl_external_scanner_serialize(void *p, char *buffer) {
  return 0;
}
void tree_sitter_kdl_external_scanner_deserialize(void *p, const char *b,
                                                  unsigned n) {}

bool scan_multi_line_comment(TSLexer *lexer) {
  if (lexer->lookahead != '/') {
    return false;
  }

  advance(lexer);

  if (lexer->lookahead != '*') {
    return false;
  }

  advance(lexer);

  bool partial_terminator = false;
  unsigned depth = 1;
  while (depth > 0) {
    if (lexer->eof(lexer)) {
      // Unclosed comment caused by EOF.
      return false;
    }

    int32_t c = lexer->lookahead;
    bool is_asterisk = c == '*';
    advance(lexer);

    if (c != '/') {
      // No change in comment depth, but might be the start of a terminator
      // ("*/").
      partial_terminator = is_asterisk;
      continue;
    }

    if (partial_terminator) {
      // Encountered "*/" sequence, reduce depth.
      depth -= 1;
      partial_terminator = is_asterisk;
      continue;
    }

    if (lexer->lookahead == '*') {
      // Encountered "/*" sequence, increase depth.
      depth += 1;
      advance(lexer);
    }
  }

  lexer->result_symbol = MULTI_LINE_COMMENT;
  return true;
}

bool scan_raw_string(TSLexer *lexer) {
  // Raw string prefix.
  if (lexer->lookahead != 'r') {
    return false;
  }

  advance(lexer);

  // First hash.
  if (lexer->lookahead != '#') {
    return false;
  }

  advance(lexer);

  // Zero or more additional hashes.
  unsigned num_hashes = 1;
  while (lexer->lookahead == '#' && num_hashes < UINT_MAX) {
    num_hashes += 1;
    advance(lexer);
  }

  // Opening quote.
  if (lexer->lookahead != '"') {
    return false;
  }

  advance(lexer);

  for (;;) {
    if (lexer->eof(lexer)) {
      // Unclosed raw string caused by EOF.
      return false;
    }

    int32_t c = lexer->lookahead;
    advance(lexer);

    if (c != '"') {
      continue;
    }

    // Try to match `num_hashes` closing hashes.
    unsigned closing_hashes = 0;
    for (;;) {
      if (lexer->lookahead != '#') {
        break;
      }

      advance(lexer);

      closing_hashes += 1;
      if (closing_hashes == num_hashes) {
        goto success;
      }
    }
  }

success:
  lexer->result_symbol = RAW_STRING;
  return true;
}

bool tree_sitter_kdl_external_scanner_scan(void *payload, TSLexer *lexer,
                                           const bool *valid_symbols) {
  if (valid_symbols[MULTI_LINE_COMMENT] && lexer->lookahead == '/') {
    return scan_multi_line_comment(lexer);
  }

  if (valid_symbols[RAW_STRING] && lexer->lookahead == 'r') {
    return scan_raw_string(lexer);
  }

  return false;
}
