<?php
// $Id$

class DrushMakeProject_Profile extends DrushMakeProject {
  public function __construct(&$project, $paths) {
    $this->project_path = 'profiles';
    parent::__construct($project, $paths);
  }
}
