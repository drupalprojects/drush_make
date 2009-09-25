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
      'drush make [script] [directory]' => 'Assemble the Drupal packages described in [script] to [directory].',
      'drush make foo.make bar' => 'Builds foo.make in the bar/ directory.',
    ),
    'arguments' => array(
      'script' => 'The path to the makefile. The path may be absolute or relative to the current working dir.',
      'directory' => 'The optional destination directory for the build. If omitted, the site will be built to the current working dir.',
    ),
    'options' => array(
      '--copy-settings' => 'Copy default.settings.php to settings.php in sites/default upon completion.',
      '--tar=[file]' => 'Generate an archive of the build at [file].tar.gz instead of to a directory.',
      '--working-copy' => 'Check out working copies of any projects that specify version control.',
      '--destination=[path]' => 'The location in the platform where modules and themes will be placed. For example "sites/all".',
      '--no-tmp' => 'Do no stage the build in a temporary directory (For internal use).',
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
 * Establish the destination paths for this build.
 *
 * @param $destination
 *   The requested destination of the build.
 *
 * @return an array of paths with keys 'base', 'build', 'temp_base', 'temp_dir', .
 */
function drush_make_get_base_path($destination = '') {
  $paths = array();

  // Determine the base of the build.
  if (drush_get_option('tar')) {
    $paths['base'] = drush_get_option('tar');
  }
  elseif (!empty($destination) || $destination == '.') {
    $paths['base'] = trim($destination, '/');
  }
  elseif(drush_confirm(dt("Make new site in the current directory?"))) {
    $paths['base'] = '.';
  }
  else {
    drush_log(dt('Build aborted.'), 'ok');
    return;
  }

  // Generate the temp directory if one is required.
  $paths['temp_base'] = $paths['base'];
  if (!drush_get_option('no-tmp')) {
    $paths['temp_dir'] = 'tmp'. time();
    if (!file_exists($paths['temp_dir'])) {
      if ($paths['base'] == '.') {
        $paths['temp_base'] = $paths['temp_dir'];
      }
      else {
        $paths['temp_base'] = "$paths[temp_dir]/$paths[base]";
      }
    }
    else {
      drush_set_error("DRUSH_MAKE_ERROR", dt("Couldn't make temporary directory."));
      return;
    }
  }

  // Determine the location within the platform for any modules and themes.
  //
  // @TODO detect if this build includes a core (ie Drupal or Pressflow) and if
  //       so set the $paths['build'] to 'sites/all' by default. For now the
  //       default is left to the base of the build to best support install
  //       profiles which are the most likely places to have makefiles.
  $build_path = drush_get_option('destination');
  if (is_string($build_path)) {
    $paths['build'] = $build_path;
  }

  return $paths;
}

/**
 * Drush callback; make based on the makefile.
 */
function drush_make($makefile = NULL, $base_path = NULL) {
  if ($info = drush_make_get_info($makefile)) {
    if ($paths = drush_make_get_base_path($base_path)) {

      drush_make_build($info, $paths['temp_base'], $paths['build']);

      // Copy settings.php.
      if (drush_get_option('copy-settings')) {
        drush_shell_exec("cp $paths[temp_base]/sites/default/default.settings.php  $paths[temp_base]/sites/default/settings.php");
      }

      // Tarring is enabled.
      if (drush_get_option('tar')) {
        drush_shell_exec("tar -Pcf -  --transform='s,$paths[temp_base],$paths[base],' $paths[temp_base] | gzip > $paths[base].tar.gz");
      }

      // If we're working in a temporary directory, move things into place and
      // clean up.
      if ($paths['temp_dir'] != $paths['base']) {
        if ($paths['base'] == '.') {
          drush_shell_exec("mv $paths[temp_base]/* .");
          drush_shell_exec("mv $paths[temp_base]/.* .");
        }
        elseif (file_exists($paths['base'])) {
          drush_set_error("DRUSH_MAKE_ERROR", dt("Could not move build into place. Directory already exists: ". $paths['base']));
          return;
        }
        else {
          drush_shell_exec("mv $paths[temp_base] $paths[base]");
        }
        drush_shell_exec("rm -r $paths[temp_dir]");
      }
      drush_log(dt("Build complete."), 'completed');
    }
  }
}

/**
 * Build a platform based on a make file.
 *
 * @param $info
 *   $info array as returned by drush_make_get_info()
 * @param $base_path
 *   The path where the platform will be assembled
 * @param $build_path
 *   Path for modules, themes and libraries that are part of the build.
 */
function drush_make_build($info, $base_path, $build_path) {

  // Retrieve project information
  $projects = array();
  foreach ($info['projects'] as $project => $project_info) {
    // Cover if there is no project info, it's just a project name.
    if (is_string($project_info) && is_numeric($project)) {
      $project = $project_info;
      $project_info = array();
    }
    // Merge the known data onto the project info.
    $project_info += array(
      'name'      => $project,
      'core'      => $info['core'],
      'base_path' => $base_path,
    );

    $project = $projects[$project_info['name']] = drush_make_project_factory($project_info, $build_path);
    if ($project->type == 'core') {
      unset($projects[$project->name]);
      $core = $project;
    }
  }

  // Ensure no errors occured before starting to build. In particular this will
  // catch situations where the update xml could not be retrieved.
  if (drush_get_error()) {
    return;
  }

  // Core is a special case, we need to set it up first thing.
  if (isset($core)) {
    $core->make();
  }
  // Build projects
  foreach ($projects as $project) {
    $project->make();
  }

  // Retrieve library information and assemble.
  if (!empty($info['libraries'])) {
    foreach ($info['libraries'] as $name => $library_info) {
      $library_info += array(
        'name'      => $name,
        'base_path' => $base_path,
        'type'      => 'library',
      );
      $library = drush_make_project_factory($library_info, $build_path);
      $library->make();
    }
  }
}

/**
 * Project object factory.
 *
 * @param $project
 *   The project info array.
 *
 * @return a DrushMakeProject_TYPE object.
 */
function drush_make_project_factory($project, $build_path) {
  $project += array(
    'version' => DRUSH_MAKE_VERSION_BEST,
    'location' => DRUSH_MAKE_UPDATE_DEFAULT_URL,
    'subdir' => '',
  );
  if (empty($project['type'])) {
    if (!($data = drush_make_download_data($project))) {
      return FALSE;
    }
    $project += $data;
  }
  if (!empty($project['type'])) {
    include_once 'drush_make.project.' . $project['type'] . '.inc';
    $class = 'DrushMakeProject_' . $project['type'];
    return new $class($project, $build_path);
  }
  return FALSE;
}

/**
 * Retrieve and process update xml.
 *
 * @param $project
 *   The project info array.
 *
 * @return
 *   Array of 'release_histroy' and 'type' info that can be added to the
 *   project info array.
 */
function drush_make_download_data($project) {
  // First, get the release history.
  @$release_history = simplexml_load_file($project['location'] . '/' . $project['name'] . '/' . $project['core']);
  if (!$release_history) {
    drush_set_error(dt("Could not retrieve version information for @project.", array('@project' => $project['name'])));
    return;
  }

  // Determine the project type.
  $term_map = array('Modules' => 'module', 'Themes' => 'theme', 'Drupal project' => 'core', 'Installation profiles' => 'profile');
  // Iterate through all terms related to this project.
  foreach ($release_history->terms->term as $term) {
    // If the term is eather 'Modules' or 'Themes', add it.
    if (in_array((string) $term->value, array_keys($term_map))) {
      return array('release_history' => $release_history, 'type' => $term_map[(string)$term->value]);
    }
  }
}

/**
 * Retrieve and parse the makefile.
 */
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
