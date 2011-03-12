<?php

/**
 * Main drush make-test callback.
 */
function drush_make_test($test) {
  if (empty($test)) {
    $rows = array();
    $rows[] = array(dt('Command'), dt('Description'));
    $rows[] = array(   '-------',     '-----------');
    foreach (drush_make_test_get_tests() as $id => $info) {
      $rows[] = array($id, $info['name']);
    }
    drush_print_table($rows, TRUE);
  }
  else if ($test === 'ALL') {
    foreach (array_keys(drush_make_test_get_tests()) as $id) {
      drush_make_test_run_test($id);
    }
  }
  else {
    $tests = explode(',', $test);
    foreach ($tests as $test) {
      drush_make_test_run_test($test);
    }
  }
}

/**
 * Get an array of all tests.
 */
function drush_make_test_get_tests($id = NULL) {
  include_once 'drush_make.parser.inc';
  $tests = drush_make_parser_parse(file_get_contents(dirname(__FILE__) . '/tests/tests.info'));
  $tests = $tests['tests'];

  if (is_string($tests)) {
    drush_set_error($tests);
    return FALSE;
  }

  if (isset($id)) {
    return isset($tests[$id]) ? $tests[$id] : FALSE;
  }
  return $tests;
}

function drush_make_test_run_test($id) {
  if ($test = drush_make_test_get_tests($id)) {
    return $test['test']($test);
  }
  drush_log(dt('Test @testname could not be found.', array('@testname' => $test['name'])), 'error');
  return FALSE;
}

/**
 * Run a makefile test.
 */
function drush_make_test_make_test($test) {
  $makefile = dirname(__FILE__) .'/'. $test['makefile'];
  $options = ($test['options'] ? $test['options'] : array()) + array('test' => TRUE, 'md5' => TRUE);

  // Log the test command.
  $debug_command = 'drush make';
  foreach ($options as $k => $v) {
    $debug_command .= _drush_escape_option($k, $v);
  }
  $debug_command .= " {$makefile}";
  drush_log(dt('Test command: @debug.', array('@debug' => $debug_command)), 'ok');

  $fail = FALSE;
  if ($data = drush_backend_invoke('make '. $makefile, $options)) {
    // Test build completion.
    if (isset($test['build'])) {
      if ($test['build'] && !empty($data['error_status'])) {
        $fail = TRUE;
        drush_log(dt('@testname: build failed.', array('@testname' => $test['name'])), 'error');
      }
      elseif (!$test['build'] && empty($data['error_status'])) {
        $fail = TRUE;
        drush_log(dt('@testname: build completed.', array('@testname' => $test['name'])), 'error');
      }
      elseif (!$test['build']) {
        $error_code =& drush_get_context('DRUSH_ERROR_CODE', DRUSH_SUCCESS);
        $error_code = DRUSH_SUCCESS;
      }
    }
    // Test for messages.
    if (isset($test['messages'])) {
      $messages = array();
      foreach ($data['log'] as $item) {
        $messages[] = $item['message'];
      }
      foreach ($test['messages'] as $message) {
        if (!in_array($message, $messages)) {
          $fail = TRUE;
          drush_log(dt('@testname: "@message" not found.', array('@testname' => $test['name'], '@message' => $message)), 'error');
        }
      }
    }
    if (!$fail) {
      drush_log(dt('Drush make test @testname passed.', array('@testname' => $test['name'])), 'ok');
      return TRUE;
    }
  }
  drush_log(dt('Drush make test @testname failed.', array('@testname' => $test['name'])), 'error');
  return FALSE;
}

function drush_make_test_parser_test($test) {
  $makefile = dirname(__FILE__) .'/'. $test['makefile'];
  include_once 'drush_make.parser.inc';
  $result = drush_make_parser_parse(file_get_contents($makefile));
  $expected_result = array(
    'one' => 'one',
    'numbers' => array(
      'three' => array('two' => 'two'),
      'four' => array('two' => 'two', 'x' => 4),
      'five' => array('two' => 'two', 'six' => 'test'),
      'eleven' => array(),
    ),
    'letters' => array(
      'a' => array('two' => 'two', 'seven' => 7)
    ),
    'projects' => array(
      'ten' => array('version' => 10),
    ),
    'eight' => 8,
    'twelve' => 12,
  );
  if (ksort($result) != ksort($expected_result)) {
    drush_log(dt('Drush make parser test failed.'), 'error');
    return FALSE;
  }
  $expected_result['projects']['nine'] = array('two' => 'two', 'version' => 9);
  $result = drush_make_parser_parse(file_get_contents($makefile), array('environment' => 'twenty'));
  if (ksort($result) != ksort($expected_result)) {
    drush_log(dt('Drush make parser test failed.'), 'error');
    return FALSE;
  }
  drush_log(dt('Drush make parser test passed.'), 'ok');
  return TRUE;
}
