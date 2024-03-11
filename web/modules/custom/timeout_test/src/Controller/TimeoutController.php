<?php

namespace Drupal\timeout_test\Controller;

use Drupal\Core\Controller\ControllerBase;

class TimeoutController extends ControllerBase {

  public function get() {
    $timeout = \Drupal::request()->query->get('timeout');
    sleep($timeout);
    return ['#markup' => $this->t('Done after @timeout seconds', ['@timeout' => $timeout])];
  }

}
