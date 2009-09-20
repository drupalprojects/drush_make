<?php

class DrushMakeDownload_CVS extends DrushMakeDownload {
  public function __construct(&$project) {
    $this->project = $project;
    $this->path = !empty($this->project->directory_name) ? $this->project->directory_name : $this->project->name;
  }

  function download() {
    if (isset($this->project->download['module'])) {
      if (!isset($this->project->download['root'])) {
        $this->project->download['root'] = "pserver:anonymous:anonymous@cvs.drupal.org:/cvs/drupal-contrib";
      }
      $flags = isset($this->project->download['revision']) ? "-r {$this->project->download['revision']}" : '';

      drush_shell_exec("mkdir {$this->path}");
      drush_shell_exec("cvs -d:{$this->project->download['root']} checkout -P -d {$this->path} {$flags} {$this->project->download['module']}");
      drush_print("{$this->project->name} checked out from {$this->project->download['module']}.");
      return $this->path;
    }
    return FALSE;
  }
}
