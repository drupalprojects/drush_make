<?php

class DrushMakeDownload_SVN extends DrushMakeDownload {
  public function __construct(&$project) {
    $this->project = $project;
    $this->path = !empty($this->project->directory_name) ? $this->project->directory_name : $this->project->name;
  }

  function download() {
    if (isset($this->project->download['url'])) {
      drush_shell_exec("svn checkout {$this->project->download['url']} {$this->path}");
      drush_print("{$this->project->name} checked out from {$this->project->download['url']}.");
      return $this->path;
    }
    return FALSE;
  }
}
