<?php

namespace Drupal\timeout_test\Form;

use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;

class TimeoutTestForm extends FormBase {

  public function getFormId() {
    return 'timeout_test_form';
  }

  public function buildForm(array $form, FormStateInterface $form_state) {

    $form['timeout'] = [
      '#type' => 'number',
      '#title' => $this->t('Timeout (seconds)'),
    ];
    $form['submit'] = [
      '#type' => 'submit',
      '#value' => $this->t('Submit'),
    ];

    return $form;
  }

  public function submitForm(array &$form, FormStateInterface $form_state) {
    $timeout = $form_state->getValue('timeout') ?: 0;
    sleep($timeout);
    $this->messenger()->addStatus($this->t('Done after @timeout seconds', ['@timeout' => $timeout]));
  }

}
