<?php
// $Id$

class DrushMakeProject_Library extends DrushMakeProject {
  public function __construct(&$project, $build_path) {
    // Allow libraries to specify where they should live in the build path.
    if (isset($project['destination'])) {
      $this->project_path = $project['destination'];
    }
    else {
      $this->project_path = 'libraries';
    }

    if (!empty($build_path)) {
      $this->project_path = $build_path .'/'. $this->project_path;
    }
    parent::__construct($project, $build_path);
  }
}
