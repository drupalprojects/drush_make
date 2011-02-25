<?php

class DrushMakeProject_Library extends DrushMakeProject {
  public function __construct(&$project, $paths) {
    // Allow libraries to specify where they should live in the build path.
    if (isset($project['destination'])) {
      $this->project_path = $project['destination'];
    }
    else {
      $this->project_path = 'libraries';
    }

    if (!empty($paths['build'])) {
      $this->project_path = $paths['build'] .'/'. $this->project_path;
    }
    parent::__construct($project, $paths);
  }
}
