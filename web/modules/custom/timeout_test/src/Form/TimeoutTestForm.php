<?php

namespace Drupal\timeout_test\Form;

use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;

/**
 * Timeout test form.
 */
class TimeoutTestForm extends FormBase {

  /**
   * {@inheritdoc}
   */
  public function getFormId() {
    return 'timeout_test_form';
  }

  /**
   * Form constructor.
   *
   * @param array $form
   *   An associative array containing the structure of the form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The current state of the form.
   *
   * @return array
   *   The form structure.
   */
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

  /**
   * Form submission handler.
   *
   * @param array $form
   *   An associative array containing the structure of the form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The current state of the form.
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $timeout = $form_state->getValue('timeout') ?: 0;
    sleep($timeout);
    $this->messenger()->addStatus($this->t('Done after @timeout seconds', ['@timeout' => $timeout]));
  }

}
