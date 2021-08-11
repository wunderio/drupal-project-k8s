<?php

namespace Drupal\testpage\Controller;

use Drupal\Core\Controller\ControllerBase;

/**
 * Provides route responses for the TestPageController module.
 */
class TestPageController extends ControllerBase {

  /**
   * Returns a simple page.
   *
   * @return array
   *   A simple renderable array.
   */
  public function testPage() {
    $element = [
      '#attached' => ['http_header' => [['Custom-Header', 'TEST123']]],
      '#markup' => 'Host: ' . \Drupal::request()->getSchemeAndHttpHost(),
    ];
    return $element;
  }

}
