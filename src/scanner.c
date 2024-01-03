#include <limits.h>
#include <stdbool.h>
#include <wctype.h>

#include "tree_sitter/parser.h"

// The order of the enum values must match the order in the grammar.js
// `externals` array. The names themselves do not matter.
enum TokenType {
  BARE_IDENTIFIER,
  DECIMAL,
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

bool is_common_space_or_newline(int32_t wc) {
  return wc == '\x09' || (wc >= '\x0A' && wc <= '\x0D') || wc == '\x20';
}

bool is_uncommon_space_or_newline(int32_t wc) {
  return wc == '\x85' || wc == L'\u00A0' || wc == L'\u1680' ||
         (wc >= L'\u2000' && wc <= L'\u200A') || wc == L'\u2028' ||
         wc == L'\u2029' || wc == L'\u202F' || wc == L'\u205F' ||
         wc == L'\u3000';
}

// clang-format off
bool is_space_or_newline(int32_t wc) {
  if (wc <= '\x20') {
    // Fast path: regular tab, newlines and space
    return is_common_space_or_newline(wc);
  } else {
    // Slow path: less common whitespace
    // TODO: should ideographic space be in the fast path?
    return is_uncommon_space_or_newline(wc);
  }
}
// clang-format on

// clang-format off
bool is_identifier(int32_t wc) {
  return (wc > '\x20' && wc < 0x110000)
    && !is_space_or_newline(wc)
    && wc != '"'
    && wc != '('
    && wc != ')'
    && wc != ','
    && !(wc >= '.' && wc <= '9') // .,0-9
    && !(wc >= ';' && wc <= '>') // ;<=>
    && !(wc >= '[' && wc <= ']') // [\]
    && wc != '{'
    && wc != '}';
}
// clang-format on

// Matches one or more digits.
bool digit1(TSLexer *lexer) {
  if (!iswdigit(lexer->lookahead)) {
    return false;
  }

  advance(lexer);

  while (iswdigit(lexer->lookahead)) {
    advance(lexer);
  }

  return true;
}

bool scan_bare_identifier_tail(TSLexer *lexer) {
  while (iswdigit(lexer->lookahead) || is_identifier(lexer->lookahead)) {
    advance(lexer);
  }

  lexer->result_symbol = BARE_IDENTIFIER;
  return true;
}

bool scan_decimal_unsigned(TSLexer *lexer) {
  if (!digit1(lexer)) {
    return false;
  }

  // Match fractional part
  if (lexer->lookahead == '.') {
    advance(lexer);

    if (!digit1(lexer)) {
      return false;
    }
  }

  // Match optional exponent
  if (lexer->lookahead == 'e' || lexer->lookahead == 'E') {
    advance(lexer);

    // Match optional sign
    if (lexer->lookahead == '-' || lexer->lookahead == '+') {
      advance(lexer);
    }

    // Match [0-9]+
    if (!digit1(lexer)) {
      return false;
    }
  }

  // Make sure we don't match the start of a different numeric literal
  int32_t c = lexer->lookahead;
  if (c == 'b' || c == 'o' || c == 'x') {
    return false;
  }

  lexer->result_symbol = DECIMAL;
  return true;
}

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

bool scan_raw_string_tail(TSLexer *lexer, unsigned num_hashes) {
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
  bool bare_ident_valid = valid_symbols[BARE_IDENTIFIER];

  // Disambiguate bare identifiers and decimals.
  if (lexer->lookahead == '+' || lexer->lookahead == '-') {
    advance(lexer);

    bool decimal_valid = valid_symbols[DECIMAL];

    if (bare_ident_valid) {
      if (iswdigit(lexer->lookahead) && decimal_valid) {
        // '+' or '-' immediately followed by a digit.
        return scan_decimal_unsigned(lexer);
      }

      // '+' or '-' not immediately followed by a digit.
      return scan_bare_identifier_tail(lexer);
    }

    if (decimal_valid) {
      // '+' or '-' where decimal is valid but ident is not.
      return scan_decimal_unsigned(lexer);
    }

    // No matching rule.
    return false;
  }

  // Disambiguate bare identifiers and raw strings.
  if (lexer->lookahead == 'r') {
    advance(lexer);

    if (valid_symbols[RAW_STRING]) {
      unsigned num_hashes = 0;
      while (lexer->lookahead == '#' && num_hashes < UINT_MAX) {
        num_hashes += 1;
        advance(lexer);
      }

      // If there are no hashes, it's a bare identifier.
      if (num_hashes == 0 && bare_ident_valid) {
        return scan_bare_identifier_tail(lexer);
      }

      // If there's at least one hash and a quote, it's a raw string.
      if (lexer->lookahead == '"') {
        advance(lexer);
        return scan_raw_string_tail(lexer, num_hashes);
      }
    }

    if (bare_ident_valid) {
      return scan_bare_identifier_tail(lexer);
    }

    return false;
  }

  if (is_identifier(lexer->lookahead) && valid_symbols[BARE_IDENTIFIER]) {
    advance(lexer);
    return scan_bare_identifier_tail(lexer);
  }

  if (iswdigit(lexer->lookahead) && valid_symbols[DECIMAL]) {
    return scan_decimal_unsigned(lexer);
  }

  if (lexer->lookahead == '/' && valid_symbols[MULTI_LINE_COMMENT]) {
    return scan_multi_line_comment(lexer);
  }

  return false;
}
