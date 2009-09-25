<?php
// $Id$

class DrushMakeProject_Theme extends DrushMakeProject {
  public function __construct(&$project, $build_path) {
    if (!empty($build_path)) {
      $this->project_path = $build_path .'/themes';
    }
    else {
        $this->project_path = 'themes';
    }
    parent::__construct($project, $build_path);
  }
}
