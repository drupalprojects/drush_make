<?php

class DrushMakeDownload_Git extends DrushMakeDownload {
  public function __construct(&$project) {
    parent::__construct($project);
  }

  function download() {
    // @TODO implement revisions, tags
    if (isset($this->project->download['url'])) {
      $this->project->download['branch'] = isset($this->project->download['branch']) ? $this->project->download['branch'] : 'master';
      drush_shell_exec("git clone {$this->project->download['url']} {$this->path}");
      drush_print("{$this->project->name} cloned from {$this->project->download['url']}.");
      return $this->path;
    }
    return FALSE;
  }
}