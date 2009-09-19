<?php
// $Id$

define('DRUSH_MAKE_UPDATE_DEFAULT_URL', 'http://updates.drupal.org/release-history');
define('DRUSH_MAKE_VERSION_BEST', 'best');

include_once 'drush_make.api.inc';

/**
 * Implementation of hook_drush_command().
 */
function drush_make_drush_command() {
  $items['make'] = array(
    'description' => 'Turns a makefile into a working drupal install.',
    'callback' => 'drush_make',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'examples' => array(
      'drush make foo.make.info' => 'Make the Drupal described in foo.make.info.',
    ),
    'arguments' => array(
      'path/to/script' => 'The path to the makefile. The path may be absolute or relative to the current working dir.',
    ),
  );
 
  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function drush_make_drush_help($section) {
  switch ($section) {
    case 'drush:make':
      return dt("@TODO: Print information on the format of the makefile here.");
  }
}

/**
 * Drush callback; make based on the makefile.
 */
function drush_make($makefile = NULL) {
  if (!($info = drush_make_get_info($makefile))) {
    return;
  }

  drush_print(dt('Getting project information...'));

  // The final array of projects.
  $projects = array(
    'drupal' => new DrushMake_Core(array(
      'name' => 'drupal',
      'core' => $info['core'],
    )),
  );

  // As a first pass, get project information..
  foreach ($info['projects'] as $project => $project_info) {
    // Cover if there is no project info, it's just a project name.
    if (is_string($project_info) && is_numeric($project)) {
      $project = $project_info;
      $project_info = array();
    }
    $project_info['name'] = $project;
    $project_info['core'] = $info['core'];

    $projects[$project_info['name']] = new DrushMake_Project($project_info);
  }

  drush_print(dt('Downloading projects...'));

  foreach ($projects as $project) {
    if ($file = $project->download()) {
      $project->move($file);
    }
  }
    
  if (drush_get_option('copy-settings')) {
    drush_shell_exec("cp sites/default/default.settings.php sites/default/settings.php");
  }

  if ($projects['drupal']->tar) {
    $drupal_filename = $projects['drupal']->tar;
    // Tarring is enabled.
    drush_op('chdir', '..');
    drush_shell_exec("tar -czf $drupal_filename.tar.gz $drupal_filename");
    drush_shell_exec('rm -r ' . $drupal_filename);
  }
}

function drush_make_get_info($makefile) {
  // Do some error checking.
  if (!isset($makefile)) {
    drush_set_error(dt('Please specify a makefile.'));
    return;
  }
  elseif (!file_exists($makefile)) {
    drush_set_error(dt('The specified makefile doesn\'t exist.'));
    return;
  }
  elseif (!($info = drush_make_parse_info_file($makefile))) {
    drush_set_error(dt('The specified makefile doesn\'t exist.'));
    return;
  }
  elseif (empty($info['projects'])) {
    drush_set_error(dt('The specified makefile doesn\'t have any projects.'));
    return;
  }
  elseif (empty($info['core'])) {
    drush_set_error(dt('The specified makefile doesn\'t have a core version.'));
    return;
  }
  return $info;
}

/**
 * Parse Drupal info file format.
 *
 * Copied verbatim from includes/common.inc
 *
 * @see drupal_parse_info_file
 */
function drush_make_parse_info_file($filename) {
  $info = array();

  if (!file_exists($filename)) {
    return $info;
  }

  $data = file_get_contents($filename);
  if (preg_match_all('
    @^\s*                           # Start at the beginning of a line, ignoring leading whitespace
    ((?:
      [^=;\[\]]|                    # Key names cannot contain equal signs, semi-colons or square brackets,
      \[[^\[\]]*\]                  # unless they are balanced and not nested
    )+?)
    \s*=\s*                         # Key/value pairs are separated by equal signs (ignoring white-space)
    (?:
      ("(?:[^"]|(?<=\\\\)")*")|     # Double-quoted string, which may contain slash-escaped quotes/slashes
      (\'(?:[^\']|(?<=\\\\)\')*\')| # Single-quoted string, which may contain slash-escaped quotes/slashes
      ([^\r\n]*?)                   # Non-quoted string
    )\s*$                           # Stop at the next end of a line, ignoring trailing whitespace
    @msx', $data, $matches, PREG_SET_ORDER)) {
    foreach ($matches as $match) {
      // Fetch the key and value string
      $i = 0;
      foreach (array('key', 'value1', 'value2', 'value3') as $var) {
        $$var = isset($match[++$i]) ? $match[$i] : '';
      }
      $value = stripslashes(substr($value1, 1, -1)) . stripslashes(substr($value2, 1, -1)) . $value3;

      // Parse array syntax
      $keys = preg_split('/\]?\[/', rtrim($key, ']'));
      $last = array_pop($keys);
      $parent = &$info;

      // Create nested arrays
      foreach ($keys as $key) {
        if ($key == '') {
          $key = count($parent);
        }
        if (!isset($parent[$key]) || !is_array($parent[$key])) {
          $parent[$key] = array();
        }
        $parent = &$parent[$key];
      }

      // Handle PHP constants
      if (defined($value)) {
        $value = constant($value);
      }

      // Insert actual value
      if ($last == '') {
        $last = count($parent);
      }
      $parent[$last] = $value;
    }
  }

  return $info;
}