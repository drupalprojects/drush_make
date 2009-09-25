<?php
// $Id$

class DrushMakeProject_Core extends DrushMakeProject {
  /**
   * The core project will always be the base of the build, so it needs to move
   * itself into place a little differently.
   */
  protected function move($project_directory) {
    drush_shell_exec("mv {$project_directory}/* {$this->base_path}");
    drush_shell_exec("mv {$project_directory}/.* {$this->base_path}");
    drush_shell_exec("rmdir {$project_directory}");
  }
}
