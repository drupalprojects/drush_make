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

function drush_make_parser_parse($tokens) {
  $commands = _drush_make_parser_extract_commands($tokens);
  $scope = array();
  $info = array();
  foreach ($commands as $command) {
    if ($command['type'] == DRUSH_MAKE_COMMAND_ASSIGNMENT) {
      if (count($command['key']) == 1) {
        $info[$command['key'][0]] = $command['value'];
      }
      else {
        if (count($command['key']) == 2 && $command['key'][1] == '') {
          $command['key'][1] = $command['value'];
          $info[$command['key'][0]][$command['value']] = array();
        }
        elseif (count($command['key']) == 2) {
          $info[$command['key'][0]][$command['key'][1]] = array('version' => $command['value']);
        }
        else {
          $current = &$info;
          foreach ($command['key'] as $key) {
            if (!isset($current[$key])) {
              $current[$key] = array();
            }
            $current = &$current[$key];
          }
          $current = $command['value'];
        }

        foreach ($scope as $item) {
          $info[$command['key'][0]][$command['key'][1]] = array_merge_recursive($item, $info[$command['key'][0]][$command['key'][1]]);
        }
      }
    }
    elseif ($command['type'] == DRUSH_MAKE_COMMAND_BEGIN_SCOPE) {
      $current_scope = array();
      foreach ($command['scope'] as $assignment) {
        $current = &$current_scope;
        foreach ($assignment['key'] as $key) {
          if (!isset($current[$key])) {
            $current[$key] = array();
          }
          $current = &$current[$key];
        }
        $current = $assignment['value'];
      }
      $scope[] = $current_scope;
    }
    elseif ($command['type'] == DRUSH_MAKE_COMMAND_END_SCOPE) {
      array_pop($scope);
    }
  }
  return $info;
}

function _drush_make_parser_extract_commands($tokens) {
  $commands = array();
  $in_comment = FALSE;

  $in_assignment = 0; // Tri-state variable: 0 - not in assignment,
                      // 1: looking for [ or =, 2: looking for ], 3: value.
  $assignment_key = array();
  $current_key = '';

  $in_scope_assignment = FALSE;
  $scope_assignment = array();

  foreach ($tokens as $token) {
    if ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && ($token['data'] == ';' || $token['data'] == '#')) {
      $in_comment = TRUE;
    }
    elseif ($in_comment && $token['type'] == DRUSH_MAKE_TOKEN_NEWLINE) {
      $in_comment = FALSE;
    }
    if (!$in_comment && $token['type'] != DRUSH_MAKE_TOKEN_NEWLINE) {

      // If it's a literal, assume it's the start of an assignment.
      if (!$in_assignment && $token['type'] == DRUSH_MAKE_TOKEN_LITERAL && $in_scope_assignment != 2) {
        // Start off the key with the current literal.
        $assignment_key = array($token['data']);
        // Set the current marker to look for a key.
        $in_assignment = 1;
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_LITERAL && $in_assignment == 2) {
        $current_key = $token['data'];
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '=' && $in_assignment == 1) {
        // If it's looking for a key and finds an equal sign, look for a value now.
        $in_assignment = 3;
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '[' && $in_assignment == 1) {
        $in_assignment = 2;
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == ']' && $in_assignment == 2) {
        $assignment_key[] = $current_key;
        $current_key = '';
        $in_key = FALSE;
        $in_assignment = 1;
      }
      elseif ($in_assignment == 1 || $in_assignment == 2) {
        return array('error' => TRUE, 'message' => 'Unexpected token "' . $token['data'] . '" in assignment.');
      }
      elseif ($in_assignment == 3) {
        if ($token['type'] == DRUSH_MAKE_TOKEN_LITERAL) {
          $assignment = array('type' => DRUSH_MAKE_COMMAND_ASSIGNMENT, 'key' => $assignment_key, 'value' => $token['data']);
          if ($in_scope_assignment) {
            $scope_assignment[] = $assignment;
          }
          else {
            $commands[] = $assignment;
          }
          $in_assignment = 0;
        }
        else {
          return array('error' => TRUE, 'message' => 'Unexpected token "' . $token['data'] . '" in assignment.');
        }
      }

      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '(' && $in_scope_assignment == 0) {
        $in_scope_assignment = 1;
      }
      elseif ($in_scope_assignment == 1 && $token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == ')') {
        $in_scope_assignment = 2;
        $commands[] = array('type' => DRUSH_MAKE_COMMAND_BEGIN_SCOPE, 'scope' => $scope_assignment);
        $scope_assignment = array();
      }
      elseif ($in_scope_assignment == 1) {
        return array('error' => TRUE, 'message' => 'Unexpected token "' . $token['data'] . '" in scope assignment.');
      }
      elseif ($in_scope_assignment == 2 && ($token['type'] != DRUSH_MAKE_TOKEN_COMMAND || $token['data'] != '{')) {
        return array('error' => TRUE, 'message' => 'A "{" must follow ")".');
      }
      elseif ($in_scope_assignment == 2 && $token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '{') {
        $in_scope_assignment = 0;
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '}') {
        $commands[] = array('type' => DRUSH_MAKE_COMMAND_END_SCOPE);
      }
    }
  }
  return $commands;
}
