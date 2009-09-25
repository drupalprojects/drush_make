<?php
// $Id$

class DrushMakeProject_Module extends DrushMakeProject {
  public function __construct(&$project, $paths) {
    if (!empty($paths['build'])) {
      $this->project_path = $paths['build'] .'/modules';
    }
    else {
        $this->project_path = 'modules';
    }
    parent::__construct($project, $paths);
  }
}
