<?php

class DrushMakeDownload_UpdateXML extends DrushMakeDownload {
  public function __construct(&$project) {
    parent::__construct($project);
  }

  function preDownload() {
    // Make an array of releases.
    foreach ($this->project->release_history->releases->release as $release) {
      $version = (string) $release->version_major;
      if ((string) $release->version_patch) {
        $version .= '.' . (string) $release->version_patch;
      }
      else {
        $version .= '.0';
      }
      if ($extra_version = (string) $release->version_extra) {
        $version .= '-' . $extra_version;
      }
      $this->releases[$version] = array(
        'file' => (string) $release->download_link,
        'md5' => (string) $release->mdhash,
        'version' => (string) $release->version,
      );
      foreach (array('major', 'patch', 'extra') as $part) {
        $this->releases[$version][$part] = (string) $release->{'version_' . $part};
      }
    }
    
    // Find the best release.
    if ($this->project->version == DRUSH_MAKE_VERSION_BEST) {
      $recommended_major = (string)$this->project->release_history->recommended_major;
      $this->project->version = $this->getBestRelease($recommended_major);
    }
    else {
      $exploded_version = explode('.', $this->project->version);
      if (count($exploded_version) == 1) {
        $this->project->version = $this->getBestRelease($exploded_version);
      }
      // Otherwise we have a full version.
    }
    $this->release = $this->releases[$this->project->version];
    $this->version = $this->releases[$this->project->version]['version'];
  }

  function download() {
    list($filename) = array_reverse(explode('/', $this->release['file']));
    // Download the project.
    if (!drush_shell_exec("wget " . $this->release['file'])) {
      drush_shell_exec("curl -O " . $this->release['file']);
    }
    if (file_exists($filename)) {
      drush_print("{$this->project->name}-{$this->version} downloaded.");
    }
    else {
      drush_set_error("Unable to download $filename from " . $this->release['file']);
      return;
    }
    
    // Check Md5 hash.
    if (md5_file($filename) != $this->release['md5']) {
      drush_op('unlink', $filename);
      drush_set_error("Error: File $filename is corrupt (wrong md5 checksum).");
      return;
    }
    
    // Unzip the file.
    drush_shell_exec('gzip --list ' . $filename);
    $gzinfo = drush_shell_exec_output();

    if (!count($gzinfo)) {
      drush_set_error(dt('Could not retrieve package information for @filename.', array('@filename' => $filename)));
      return;
    }
    
    // Unzip it and then delete the tar file.
    drush_shell_exec('gzip -d ' . $filename);
    
    $gzinfo = array_pop($gzinfo);
    $matches = array();
    preg_match('/[a-zA-Z0-9.\-_,]*.tar/', $gzinfo, $matches);
    if (!isset($matches[0])) {
      drush_set_error(dt('Could not retrieve tar filename from @filename for @project.', array('@project' => $this->project->name, '@filename' => $filename)));
      return;
    }
    $file = $matches[0];

    // Get the filename once unzipped.
    drush_shell_exec('tar -tf ' . $file);
    list($project_directory) = drush_shell_exec_output();
    drush_shell_exec('tar -xf ' . $file);
    drush_op('unlink', $file);

    return $project_directory;
  }

  protected function getBestRelease($major) {
    $best = NULL;
    foreach ($this->releases as $version => $release_info) {
      if ($release_info['major'] == $major && version_compare($best, $version, '<')) {
        $best = $version;
      }
    }
    return $best;
  }
}
