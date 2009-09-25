<?php
// $Id$

class DrushMakeProject_Theme extends DrushMakeProject {
  public function __construct(&$project, $paths) {
    if (!empty($paths['build'])) {
      $this->project_path = $paths['build'] .'/themes';
    }
    else {
        $this->project_path = 'themes';
    }
    parent::__construct($project, $paths);
  }
}
