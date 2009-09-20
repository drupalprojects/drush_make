<?php
// $Id$

/**
 * The base project class.
 */
class DrushMakeProject {
  var $base_path = '.';
  var $name = '';
  var $version = DRUSH_MAKE_VERSION_BEST;
  var $location = DRUSH_MAKE_UPDATE_DEFAULT_URL;
  var $subdir = '';
  var $directory_name = '';
  var $core = '';
  var $download;

  /**
   * Set attributes and retrieve project information.
   */
  function __construct($project) {
    foreach ($project as $key => $value) {
      $this->{$key} = $value;
    }
    $this->downloadClass = DrushMakeDownload::factory($this);
    if (!$this->downloadClass) {
      drush_error(dt('Could not find download type @type', array('@type' => $this->download['type'])));
    }
  }

  /**
   * Build a project.
   */
  function make() {
    if ($directory = $this->downloadClass->download()) {
      $path = $this->move($directory);
      $this->applyPatches($path);
    }
  }

  /**
   * Retrieve and apply any patches specified by the makefile to this project.
   */
  protected function applyPatches($project_directory) {
    if (!empty($this->patch)) {
      foreach ($this->patch as $url) {
        // Download the patch.
        list($filename) = array_reverse(explode('/', $url));
        if (!drush_shell_exec("wget {$url}")) {
          drush_shell_exec("curl -O {$url}");
        }
        if (file_exists($filename)) {
          drush_shell_exec("mv {$filename} {$project_directory}");
          drush_shell_exec("patch -p0 -d {$project_directory} < {$project_directory}/{$filename}");
          drush_print("  {$this->name} patched with {$filename}.");
        }
        else {
          drush_set_error("  Unable to download $filename from $url.");
        }
      }
    }
  }

  /**
   * Move a project to its final resting place.
   */
  protected function move($project_directory) {
    $path = $this->generatePath();
    if ($this->directory_name) {
      $path .= '/' . $this->directory_name;
    }
    else {
      $path .= '/' . trim($project_directory, '/');
    }
    drush_shell_exec("mv {$project_directory} {$path}");
    return $path;
  }

  /**
   * Generate the proper path for this project type.
   */
  protected function generatePath() {
    switch ($this->type) {
      case 'profile':
        $path = $this->base_path . '/profiles';
        break;
      case 'module':
        $path = $this->base_path . '/sites/all/modules';
        break;
      case 'theme':
      case 'theme engine':
        $path = $this->base_path . '/sites/all/themes';
        break;
      // @TODO
      case 'translation':
        break;
    }
    // If directory doesn't exist, create it.
    if (!is_dir($path)) {
      drush_op('mkdir', $path);
    }
    // Generate subdirectory paths.
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
}

class DrushMakeProject_Core extends DrushMakeProject {
  protected function move($project_directory) {
    if ($this->base_path == '.') {
      drush_shell_exec("mv {$project_directory}/* {$this->base_path}");
      drush_shell_exec("mv {$project_directory}/.* {$this->base_path}");
      drush_shell_exec("rmdir {$project_directory}");
    }
    else {
      drush_shell_exec("mv {$project_directory} {$this->base_path}");
    }
  }
}

class DrushMakeDownload {
  protected function __construct() {}

  function factory($project) {
    if (empty($project->download) || empty($project->download['type'])) {
      $project->download = array('type' => 'updatexml');
    }
    include_once 'drush_make.download.' . $project->download['type'] . '.inc';
    $class = 'DrushMakeDownload_' . $project->download['type'];
    if (class_exists($class)) {
      return new $class($project);
    }
    else {
      return FALSE;
    }
  }

  function download() {
    return FALSE;
  }
}
