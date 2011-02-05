<?php
// $Id$

function drush_make_parser_tokenize($data) {
  $chunks = _drush_make_parser_extract_literals($data);
  $in_quote = FALSE;
  $tokens = array();

  $last_newline = TRUE; // Prevent double newlines.
  foreach ($chunks as $chunk) {
    if (!$in_quote) {
      _drush_make_parser_tokenize($chunk, $tokens, $last_newline);
    }
    elseif (!empty($chunk)) {
      $last_newline = FALSE;
      $tokens[] = array('type' => DRUSH_MAKE_TOKEN_LITERAL, 'data' => $chunk);
    }
    $in_quote = !$in_quote;
  }
  return $tokens;
}

function _drush_make_parser_extract_literals($data) {
  $in_quote = FALSE;
  $chunk = '';
  $chunks = array();
  for ($position = 0; $position < strlen($data); $position++) {
    $current = $data{$position};
    if (($current == '"' || $current == "'") && !$in_quote) {
      $chunks[] = $chunk;
      $chunk = '';
      $in_quote = $current;
    }
    elseif ($in_quote && $current == $in_quote && $data{$position - 1} != '\\') {
      $chunks[] = $chunk;
      $chunk = '';
      $in_quote = FALSE;
    }
    else {
      $chunk .= $current;
    }
  }
  $chunks[] = $chunk;
  return $chunks;
}

function _drush_make_parser_tokenize($data, &$tokens, &$last_newline) {
  $token = '';
  $command_characters = array('[', ']', '{', '}', '(', ')', ';', '#', '=', ',');
  $whitespace_characters = array(' ', "\n", "\t");

  for ($position = 0; $position < strlen($data); $position++) {
    $current = $data{$position};

    if (in_array($current, $command_characters)) {
      if (!empty($token)) {
        $tokens[] = array('type' => DRUSH_MAKE_TOKEN_LITERAL, 'data' => $token);
        $token = '';
      }
      $tokens[] = array('type' => DRUSH_MAKE_TOKEN_COMMAND, 'data' => $current);
      $last_newline = FALSE;
    }
    elseif (in_array($current, $whitespace_characters)) {
      if (!empty($token)) {
        $last_newline = FALSE;
        $tokens[] = array('type' => DRUSH_MAKE_TOKEN_LITERAL, 'data' => $token);
        $token = '';
      }
      if ($current == "\n" && !$last_newline) {
        $tokens[] = array('type' => DRUSH_MAKE_TOKEN_NEWLINE);
        $last_newline = TRUE;
      }
    }
    else {
      $token .= $current;
    }
  }
  if (!empty($token)) {
    $tokens[] = array('type' => DRUSH_MAKE_TOKEN_LITERAL, 'data' => $token);
  }
}
