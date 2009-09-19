<?php

class DrushMake_Project {
  var $name = '';
  var $version = DRUSH_MAKE_VERSION_BEST;
  var $location = DRUSH_MAKE_UPDATE_DEFAULT_URL;
  var $subdir = '';
  var $directory_name = '';
  var $core = '';

  function __construct($project) {
    foreach ($project as $key => $value) {
      $this->{$key} = $value;
    }
    $this->getInfo();
  }

  function download() {
    list($filename) = array_reverse(explode('/', $this->release['file']));
    // Download the project.
    if (!drush_shell_exec("wget " . $this->release['file'])) {
      drush_shell_exec("curl -O " . $this->release['file']);
    }
    if (file_exists($filename)) {
      drush_print("{$this->name}-{$this->version} downloaded.");
    }
    else {
      drush_set_error("Unable to download $filename from " . $this->release['file']);
      return;
    }

    // Check Md5 hash.
    if (md5_file($filename) != $this->release['md5']) {
      drush_op('unlink', $filename);
      drush_set_error("Error: File $filename is corrupt (wrong md5 checksum).");
      return;
    }

    // Unzip the file.
    drush_shell_exec('gzip --list ' . $filename);
    $gzinfo = drush_shell_exec_output();

    if (!count($gzinfo)) {
      drush_set_error(dt('Could not retrieve package information for @filename.', array('@filename' => $filename)));
      return;
    }

    // Unzip it and then delete the tar file.
    drush_shell_exec('gzip -d ' . $filename);

    $gzinfo = array_pop($gzinfo);
    $matches = array();
    preg_match('/[a-zA-Z0-9.\-_,]*.tar/', $gzinfo, $matches);
    if (!isset($matches[0])) {
      drush_set_error(dt('Could not retrieve tar filename from @filename for @project.', array('@project' => $project, '@filename' => $filename)));
      return;
    }

    return $matches[0];
  }

  function move($file) {
    $path = $this->generatePath();
    // Get the filename once unzipped.
    drush_shell_exec('tar -tf ' . $file);
    list($project_directory) = drush_shell_exec_output();
    drush_shell_exec('tar -xf ' . $file);
    drush_op('unlink', $file);

    if ($this->directory_name) {
      $path .= '/' . $this->directory_name;
    }
    else {
      $path .= '/' . trim($project_directory, '/');
    }
    drush_shell_exec('mv ' . $project_directory . ' ' .  $path);
  }

  protected function generatePath() {
    // Assume we're in a Drupal root at this point.
    $path = 'sites/all/' . $this->type . 's';
    // If sites/all/[project type] doesn't exist, create it.
    if (!is_dir($path)) {
      drush_op('mkdir', $path);
    }

    if (!empty($this->subdir)) {
      foreach (explode('/', $this->subdir) as $part) {
        if (!is_dir($part)) {
          drush_op('mkdir', $part);
        }
        $path .= '/' . $part;
      }
    }
    return $path;
  }

  protected function getInfo() {
    // First, get the release history.
    @$this->release_history = simplexml_load_file($this->location . '/' . $this->name . '/' . $this->core);
    if (!$this->release_history) {
      drush_set_error(dt("Could not retrieve version information for @project.", array('@project' => $project)));
      return;
    }

    // Determine the project type.
    $term_map = array('Modules' => 'module', 'Themes' => 'theme');
    // Iterate through all terms related to this project.
    foreach ($this->release_history->terms->term as $term) {
      // If the term is eather 'Modules' or 'Themes', add it.
      if (in_array((string) $term->value, array_keys($term_map))) {
        $this->type = $term_map[(string)$term->value];
        break;
      }
    }

    // Make an array of releases.
    foreach ($this->release_history->releases->release as $release) {
      $version = (string) $release->version_major;
      if ((string) $release->version_patch === '0' || (string) $release->version_patch) {
        $version .= '.' . (string) $release->version_patch;
      }
      if ($extra_version = (string) $release->version_extra) {
        $version .= '-' . $extra_version;
      }
      $this->releases[$version] = array(
        'file' => (string) $release->download_link,
        'md5' => (string) $release->mdhash,
      );
      foreach (array('major', 'patch', 'extra') as $part) {
        $this->releases[$version][$part] = (string) $release->{'version_' . $part};
      }
    }

    // Find the best release.
    if ($this->version == DRUSH_MAKE_VERSION_BEST) {
      $recommended_major = (string)$this->release_history->recommended_major;
      $this->version = $this->getBestRelease($recommended_major);
    }
    else {
      $exploded_version = explode('.', $this->version);
      if (count($exploded_version) == 1) {
        $this->version = $this->getBestRelease($exploded_version);
      }
      // Otherwise we have a full version.
    }
    $this->release = $this->releases[$this->version];
  }

  protected function getBestRelease($major) {
    $best = NULL;
    foreach ($this->releases as $version => $release_info) {
      if ($release_info['major'] == $major && version_compare($best, $version, '<')) {
        $best = $version;
      }
    }
    return $best;
  }
}

class DrushMake_Core extends DrushMake_Project {
  function move($file) {
    // Get the filename once unzipped.
    drush_shell_exec('tar -tf ' . $file);
    $output = drush_shell_exec_output();
    drush_shell_exec('tar -xf ' . $file);
    drush_op('unlink', $file);
    if ($tar = drush_get_option('tar')) {
      $file = trim(array_shift($output), '/');
      drush_shell_exec('mv ' . $file . ' ' . $tar);
      chdir($tar);
      return $tar;
    }
    else {
      foreach ($output as $item) {
        $parts = explode('/', trim($item, '/'));
        if (count($parts) == 2) {
          drush_shell_exec('mv ' . $item . ' ' . $parts[1]);
        }
        elseif (count($parts) == 1) {
          $root = $parts[0];
        }
      }
      drush_shell_exec('rm -r ' . $root);
    }
  }
}
