<?php

define('DRUSH_MAKE_TOKEN_LITERAL', 'literal');
define('DRUSH_MAKE_TOKEN_COMMAND', 'command');
define('DRUSH_MAKE_TOKEN_NEWLINE', 'newline');

define('DRUSH_MAKE_COMMAND_ASSIGNMENT', 'assignment');
define('DRUSH_MAKE_COMMAND_CONDITION', 'condition');
define('DRUSH_MAKE_COMMAND_BEGIN_SCOPE', 'begin scope');
define('DRUSH_MAKE_COMMAND_END_SCOPE', 'end scope');

function drush_make_parser_parse($makefile, $conditions = array(), $options = array(), $scope = array()) {
  if (!($data = drush_make_get_data($makefile))) {
    return drush_set_error(dt('Invalid or empty make file: %makefile', array('%makefile' => $makefile)));
  }

  $options += array('top level' => array());
  $tokens = _drush_make_parser_tokenize($data);
  $commands = _drush_make_parser_extract_commands($tokens);
  $info = _drush_make_parser_interpret($commands, $conditions, $options, $scope);
  if (!is_array($info) || empty($info['includes'])) {
    return $info;
  }

  foreach ($info['includes'] as $include => $include_object) {
    $file = (string)$include;
    if (drush_make_valid_url($file, TRUE) || file_exists($options['include_path'] . '/' . $file)) {
      if (file_exists($options['include_path'] . '/' . $file)) {
        $file = $options['include_path'] . '/' . $file;
      }
      $included_info = drush_make_parser_parse($file, $include_object->getConditions(), $include_object->getOptions(), $include_object->getScope());
      if (!is_array($included_info)) {
        return $included_info;
      }

      foreach ($included_info as $key => $value) {
        if (isset($included_info[$key]) && isset($info[$key])) {
          if (is_array($info[$key]) && is_array($included_info[$key])) {
            $info[$key] = array_merge($included_info[$key], $info[$key]);
          }
          elseif (!is_array($info[$key]) && !is_array($included_info[$key])) {
            $info[$key] = $included_info[$key];
          }
          else {
            return dt('Included makefile disagrees about whether @key is an array.', array('@key' => $key));
          }
        }
        else {
          $info[$key] = $included_info[$key];
        }
      }
    }
  }

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

function _drush_make_parser_interpret($commands, $conditions, $options, $scope) {
  if (is_string($commands)) {
    return $commands;
  }
  $scope = array($scope);
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
          $info[$command['key'][0]][$command['value']] = new DrushMakeParserObject($scope, $conditions, $options);
        }
        elseif (count($command['key']) == 2 && in_array($command['key'][0], $options['top level'])) {
          $info[$command['key'][0]][$command['key'][1]] = new DrushMakeParserObject($scope, $conditions, $options);
          $info[$command['key'][0]][$command['key'][1]]['version'] = _drush_make_parser_value($command['value']);
        }
        else {
          _drush_make_parser_assign($info, $command, $scope, $conditions, $options);
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

function _drush_make_parser_assign(&$current, $assignment, $scope = FALSE, $conditions = array(), $options = array()) {
  $level = 0;

  while ($key = array_shift($assignment['key'])) {
    if ($key != '' && !isset($current[$key])) {
      if ($level == 1 && $scope !== FALSE) {
        $current[$key] = new DrushMakeParserObject($scope, $conditions, $options);
      }
      else {
        $current[$key] = array();
      }
    }
    if ($key == '') {
      $current = &$current[];
      if ($level == 1 && $scope !== FALSE) {
        $current = new DrushMakeParserObject($scope, $conditions, $options);
      }
    }
    else {
      $current = &$current[$key];
    }
    if ($current instanceof DrushMakeParserObject && $scope !== FALSE) {
      $current->setValue($assignment['key'], _drush_make_parser_value($assignment['value']));
      return;
    }
    $level++;
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

function drush_make_parser_to_array($info) {
  foreach ($info as $top_key => $top_value) {
    if (!is_array($top_value)) {
      continue;
    }
    foreach ($top_value as $key => $value) {
      if ($value instanceof DrushMakeParserObject) {
        $info[$top_key][$key] = $value->toValue();
      }
    }
  }
  return $info;
}

class DrushMakeParserObject extends ArrayObject {
  protected $scope = array();
  protected $store = array();
  protected $conditions = array();
  protected $options = array();
  function __construct($scopes, $conditions, $options) {
    // Normalize scope.
    foreach ($scopes as $scope) {
      $this->scope = array_merge_recursive($scope, $this->scope);
    }
    $this->options = $options;
    $this->conditions = $conditions;
  }
  function getScope() {
    return $this->scope;
  }
  function getConditions() {
    return $this->conditions;
  }
  function getOptions() {
    return $this->options;
  }
  function offsetGet($offset) {
    if (!is_array($this->store)) {
      return;
    }
    if (isset($this->store[$offset])) {
      return $this->store[$offset];
    }
    elseif (isset($this->scope[$offset])) {
      return $this->scope[$offset];
    }
  }
  function offsetExists($offset) {
    if (!is_array($this->store)) {
      return;
    }
    return isset($this->store[$offset]);
  }
  function offsetSet($offset, $value) {
    if (!is_array($this->store)) {
      return;
    }
    $this->store[$offset] = $value;
  }
  function offsetUnset($offset) {
    if (!is_array($this->store)) {
      return;
    }
    unset($this->store[$offset]);
  }
  function toValue() {
    if (is_array($this->store)) {
      return array_merge_recursive($this->store, $this->scope);
    }
    else {
      return $this->store;
    }
  }
  function setValue($keys, $value) {
    if (empty($keys)) {
      $this->store = $value;
    }
    else {
      $key = array_shift($keys);
      $current = &$this->store[$key];
      foreach ($keys as $key) {
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
      $current = $value;
    }
  }
  function __toString() {
    if (is_string($this->store)) {
      return $this->store;
    }
  }
}
