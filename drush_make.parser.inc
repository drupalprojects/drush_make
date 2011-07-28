<?php

define('DRUSH_MAKE_TOKEN_LITERAL', 'literal');
define('DRUSH_MAKE_TOKEN_COMMAND', 'command');
define('DRUSH_MAKE_TOKEN_NEWLINE', 'newline');

define('DRUSH_MAKE_COMMAND_ASSIGNMENT', 'assignment');
define('DRUSH_MAKE_COMMAND_CONDITION', 'condition');
define('DRUSH_MAKE_COMMAND_BEGIN_SCOPE', 'begin scope');
define('DRUSH_MAKE_COMMAND_END_SCOPE', 'end scope');

function drush_make_parser_parse($data, $conditions = array()) {
  $tokens = _drush_make_parser_tokenize($data);
  $commands = _drush_make_parser_extract_commands($tokens);
  $info = _drush_make_parser_interpret($commands, $conditions);
  return $info;
}

function _drush_make_parser_tokenize($data) {
  $chunks = _drush_make_parser_extract_literals($data);
  $in_quote = FALSE;
  $tokens = array();

  $last_newline = TRUE; // Prevent double newlines.
  foreach ($chunks as $chunk) {
    if (!$in_quote) {
      _drush_make_parser_tokenize_helper($chunk, $tokens, $last_newline);
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

function _drush_make_parser_tokenize_helper($data, &$tokens, &$last_newline) {
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

function _drush_make_parser_extract_commands($tokens) {
  $commands = array();
  $in_comment = FALSE;

  $in_assignment = 0; // Tri-state variable: 0 - not in assignment,
                      // 1: looking for [ or =, 2: looking for ], 3: value.
  $assignment_key = array();
  $current_key = '';

  $in_scope_assignment = FALSE;
  $scope_assignment = array();
  $scope = 0;

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
        return 'Unexpected token "' . $token['data'] . '" in assignment.';
      }
      elseif ($in_assignment == 3) {
        if ($token['type'] == DRUSH_MAKE_TOKEN_LITERAL) {
          $assignment = array('type' => DRUSH_MAKE_COMMAND_ASSIGNMENT, 'key' => $assignment_key, 'value' => $token['data']);
          if ($in_scope_assignment) {
            if (strpos($assignment['key'][0], ':') === 0) {
              $assignment['type'] = DRUSH_MAKE_COMMAND_CONDITION;
              $assignment['key'][0] = substr($assignment['key'][0], 1);
            }
            $scope_assignment[] = $assignment;
          }
          else {
            $commands[] = $assignment;
          }
          $in_assignment = 0;
        }
        else {
          return 'Unexpected token "' . $token['data'] . '" in assignment.';
        }
      }

      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '(' && $in_scope_assignment == 0) {
        $in_scope_assignment = 1;
      }
      elseif ($in_scope_assignment == 1 && $token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == ')') {
        $in_scope_assignment = 2;
        $commands[] = array('type' => DRUSH_MAKE_COMMAND_BEGIN_SCOPE, 'scope' => $scope_assignment);
        $scope_assignment = array();
        $scope++;
      }
      elseif ($in_scope_assignment == 1 && $token['data'] != ',') {
        return 'Unexpected token "' . $token['data'] . '" in scope assignment.';
      }
      elseif ($in_scope_assignment == 2 && ($token['type'] != DRUSH_MAKE_TOKEN_COMMAND || $token['data'] != '{')) {
        return 'A "{" must follow ")".';
      }
      elseif ($in_scope_assignment == 2 && $token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '{') {
        $in_scope_assignment = 0;
      }
      elseif ($token['type'] == DRUSH_MAKE_TOKEN_COMMAND && $token['data'] == '}') {
        $commands[] = array('type' => DRUSH_MAKE_COMMAND_END_SCOPE);
        $scope--;
      }
    }
  }
  if ($scope != 0) {
    return 'Scope mismatch';
  }

  return $commands;
}

function _drush_make_parser_interpret($commands, $conditions) {
  if (is_string($commands)) {
    return $commands;
  }
  $scope = array();
  $info = array();
  $merged = array();
  while (count($commands)) {
    $command = array_shift($commands);
    if ($command['type'] == DRUSH_MAKE_COMMAND_ASSIGNMENT) {
      if (count($command['key']) == 1) {
        $info[$command['key'][0]] = $command['value'];
      }
      else {
        if (count($command['key']) == 2 && $command['key'][1] == '') {
          $command['key'][1] = $command['value'];
          $info[$command['key'][0]][$command['value']] = array();
        }
        elseif (count($command['key']) == 2 && in_array($command['key'][0], _drush_make_top_level_containers())) {
          $info[$command['key'][0]][$command['key'][1]] = array('version' => _drush_make_parser_value($command['value']));
        }
        else {
          _drush_make_parser_assign($info, $command);
        }

        if (!in_array(array($command['key'][0], $command['key'][1]), $merged)) {
          foreach ($scope as $item) {
            $info[$command['key'][0]][$command['key'][1]] = array_merge_recursive($item, $info[$command['key'][0]][$command['key'][1]]);
          }
          $merged[] = array($command['key'][0], $command['key'][1]);
        }
      }
    }
    elseif ($command['type'] == DRUSH_MAKE_COMMAND_BEGIN_SCOPE) {
      $current_scope = array();
      $conditions_passed = TRUE;
      foreach ($command['scope'] as $scope_item) {
        if ($scope_item['type'] == DRUSH_MAKE_COMMAND_ASSIGNMENT) {
          _drush_make_parser_assign($current_scope, $scope_item);
        }
        elseif ($scope_item['type'] == DRUSH_MAKE_COMMAND_CONDITION &&
          (!isset($conditions[$scope_item['key'][0]]) || $conditions[$scope_item['key'][0]] != $scope_item['value'])) {
            // Scope doesn't match.
          while ($command['type'] != DRUSH_MAKE_COMMAND_END_SCOPE) {
            $command = array_shift($commands);
          }
          $conditions_passed = FALSE;
        }
      }
      if ($conditions_passed) {
        $scope[] = $current_scope;
      }
    }
    elseif ($command['type'] == DRUSH_MAKE_COMMAND_END_SCOPE) {
      array_pop($scope);
    }
  }
  return $info;
}

function _drush_make_parser_assign(&$current, $assignment) {
  foreach ($assignment['key'] as $key) {
    if ($key != '' && !isset($current[$key])) {
      $current[$key] = array();
    }
    if ($key == '') {
      $current = &$current[];
    }
    else {
      $current = &$current[$key];
    }
  }
  $current = _drush_make_parser_value($assignment['value']);
}

function _drush_make_parser_value($value) {
  $constants = get_defined_constants();
  if (isset($constants[$value])) {
    $value = $constants[$value];
  }
  return $value;
}