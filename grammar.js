module.exports = grammar({
  name: "kdl",
  externals: $ => [
    $.bare_identifier,
    $.decimal,
    $.multi_line_comment,
    $.raw_string,
  ],
  rules: {
    source_file: $ => seq(
      repeat($.node),
      repeat($._linespace),
    ),

    node: $ => seq(
      repeat($._linespace),
      field("type", optional($.type)),
      field("name", $.identifier),
      field("props", repeat(seq(
        repeat1($._node_space),
        $._node_prop_or_arg,
      ))),
      field("children", optional(seq(
        repeat($._node_space),
        $.node_children,
      ))),
      repeat($._node_space),
      $._node_terminator,
    ),

    _node_prop_or_arg: $ => choice($.value, $.prop),

    node_children: $ => seq(
      "{",
      repeat($.node),
      repeat($._linespace),
      "}",
    ),

    _node_space: $ => choice(
      $._escline,
      $._ws,
    ),

    _node_terminator: $ => choice(
      $.single_line_comment,
      $._newline,
      ";",
    ),

    identifier: $ => choice($.keyword, $.string, $.bare_identifier),
    keyword: $ => choice($.boolean, "null"),
    prop: $ => seq($.identifier, "=", $.value),
    value: $ => seq(
      optional($.type),
      choice($.string, $.number, $.keyword),
    ),
    type: $ => seq("(", $.identifier, ")"),

    // Strings =====================================================================================

    string: $ => choice(
      $.raw_string,
      $.escaped_string,
    ),

    escaped_string: $ => seq(
      field("open", "\""),
      field("contents", repeat($._character)),
      field("close", "\""),
    ),
    _character: $ => choice(
      $.escape_sequence,
      /[^\\"]/,
    ),
    escape_sequence: $ => seq("\\", $.escape_code),
    escape_code: $ => choice(
      /["\\/bfnrt]/,
      $.unicode_escape,
    ),
    unicode_escape: $ => seq("u{", repeat($._hex_digit), "}"),
    _hex_digit: $ => /[0-9A-Fa-f]/,

    // Numerics and booleans =======================================================================

    number: $ => choice($.decimal),

    boolean: $ => choice("true", "false"),

    // Whitespace and comments =====================================================================

    _escline: $ => seq(
      "\\",
      repeat($._ws),
      choice($.single_line_comment, $._newline),
    ),

    _linespace: $ => choice(
      $._newline,
      $._ws,
      $.single_line_comment,
    ),

    _newline: $ => /\r\n|[\u000A\u000C\u000D\u0085\u2028\u2029]/,
    _ws: $ => choice(
      $._bom,
      $._unicode_space,
      $.multi_line_comment,
    ),
    _bom: $ => "\uFEFF",
    _unicode_space: $ => /[\u0009\u0020\u00A0\u1680\u2000-\u200A\u202F\u205F\u3000]/,

    single_line_comment: $ => seq(
      "//",
      /[^\u000A\u000C\u000D\u0085\u2028\u2029]*/,
      $._newline,
    ),
  },
  extras: $ => [],
});
