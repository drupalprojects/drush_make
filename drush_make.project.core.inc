<?php

class DrushMakeProject_Core extends DrushMakeProject {
  public function __construct(&$project) {
    parent::__construct($project);
  }
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