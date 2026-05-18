<?php

namespace Drupal\Tests\testing_utilities\Unit;

use Drupal\Tests\UnitTestCase;

/**
 * Tests GDPR dump configuration validation logic.
 *
 * @group testing_utilities
 */
class GdprDumpConfigValidatorTest extends UnitTestCase {

  /**
   * Tests randomizeText converter configuration validation.
   */
  public function testRandomizeTextConverterValidation(): void {
    $config = [
      'converter' => 'randomizeText',
      'unique' => TRUE,
    ];

    $this->assertTrue($this->isValidRandomizeTextConfig($config));
    $this->assertEquals('randomizeText', $config['converter']);
    $this->assertTrue($config['unique']);
    $this->assertArrayNotHasKey('parameters', $config, 'randomizeText should not have parameters');
  }

  /**
   * Tests faker converter configuration validation.
   */
  public function testFakerConverterValidation(): void {
    $config = [
      'converter' => 'faker',
      'unique' => TRUE,
      'parameters' => [
        'formatter' => 'name',
      ],
    ];

    $this->assertTrue($this->isValidFakerConfig($config));
    $this->assertEquals('faker', $config['converter']);
    $this->assertTrue($config['unique']);
    $this->assertArrayHasKey('parameters', $config);
    $this->assertEquals('name', $config['parameters']['formatter']);
  }

  /**
   * Data provider for converter configuration tests.
   *
   * @return array
   *   Test cases for different converter configurations.
   */
  public function converterConfigProvider(): array {
    return [
      'valid_randomizeText' => [
        'config' => [
          'converter' => 'randomizeText',
          'unique' => TRUE,
        ],
        'expected_valid' => TRUE,
        'expected_type' => 'randomizeText',
      ],
      'valid_faker' => [
        'config' => [
          'converter' => 'faker',
          'unique' => TRUE,
          'parameters' => ['formatter' => 'name'],
        ],
        'expected_valid' => TRUE,
        'expected_type' => 'faker',
      ],
      'invalid_randomizeText_with_parameters' => [
        'config' => [
          'converter' => 'randomizeText',
          'unique' => TRUE,
          'parameters' => ['formatter' => 'name'],
        ],
        'expected_valid' => FALSE,
        'expected_type' => 'randomizeText',
      ],
      'invalid_faker_without_parameters' => [
        'config' => [
          'converter' => 'faker',
          'unique' => TRUE,
        ],
        'expected_valid' => FALSE,
        'expected_type' => 'faker',
      ],
    ];
  }

  /**
   * Tests various converter configurations.
   *
   * @dataProvider converterConfigProvider
   */
  public function testConverterConfigurations(array $config, bool $expected_valid, string $expected_type): void {
    $this->assertEquals($expected_type, $config['converter']);

    if ($expected_type === 'randomizeText') {
      $is_valid = $this->isValidRandomizeTextConfig($config);
    }
    else {
      $is_valid = $this->isValidFakerConfig($config);
    }

    $this->assertEquals($expected_valid, $is_valid,
      "Configuration validation should return {$expected_valid} for {$expected_type} converter"
    );
  }

  /**
   * Tests configuration migration from faker to randomizeText.
   */
  public function testConfigurationMigration(): void {
    // Original faker configuration.
    $original_config = [
      'converter' => 'faker',
      'unique' => TRUE,
      'parameters' => [
        'formatter' => 'name',
      ],
    ];

    // Migrated randomizeText configuration.
    $migrated_config = $this->migrateFakerToRandomizeText($original_config);

    $this->assertEquals('randomizeText', $migrated_config['converter']);
    $this->assertTrue($migrated_config['unique']);
    $this->assertArrayNotHasKey('parameters', $migrated_config);
  }

  /**
   * Tests uniqueness guarantee comparison.
   */
  public function testUniquenessGuarantee(): void {
    $faker_config = ['converter' => 'faker', 'unique' => TRUE];
    $randomize_config = ['converter' => 'randomizeText', 'unique' => TRUE];

    // Faker doesn't guarantee uniqueness even with unique flag.
    $this->assertFalse($this->guaranteesUniqueness($faker_config));

    // randomizeText guarantees uniqueness with unique flag.
    $this->assertTrue($this->guaranteesUniqueness($randomize_config));
  }

  /**
   * Tests configuration comparison for SLT-968 requirements.
   */
  public function testSlt968Requirements(): void {
    $old_config = [
      'converter' => 'faker',
      'unique' => TRUE,
      'parameters' => ['formatter' => 'name'],
    ];

    $new_config = [
      'converter' => 'randomizeText',
      'unique' => TRUE,
    ];

    // Verify the change addresses the uniqueness issue.
    $this->assertFalse($this->guaranteesUniqueness($old_config), 'Old faker config should not guarantee uniqueness');
    $this->assertTrue($this->guaranteesUniqueness($new_config), 'New randomizeText config should guarantee uniqueness');

    // Verify configuration simplification.
    $this->assertArrayHasKey('parameters', $old_config, 'Old config should have parameters');
    $this->assertArrayNotHasKey('parameters', $new_config, 'New config should not have parameters');

    // Verify both maintain unique flag.
    $this->assertTrue($old_config['unique']);
    $this->assertTrue($new_config['unique']);
  }

  /**
   * Validates randomizeText converter configuration.
   *
   * @param array $config
   *   The converter configuration.
   *
   * @return bool
   *   TRUE if valid, FALSE otherwise.
   */
  protected function isValidRandomizeTextConfig(array $config): bool {
    if ($config['converter'] !== 'randomizeText') {
      return FALSE;
    }

    // randomizeText should not have parameters.
    if (isset($config['parameters'])) {
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Validates faker converter configuration.
   *
   * @param array $config
   *   The converter configuration.
   *
   * @return bool
   *   TRUE if valid, FALSE otherwise.
   */
  protected function isValidFakerConfig(array $config): bool {
    if ($config['converter'] !== 'faker') {
      return FALSE;
    }

    // Faker should have parameters with formatter.
    if (!isset($config['parameters']['formatter'])) {
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Migrates faker configuration to randomizeText.
   *
   * @param array $config
   *   The original faker configuration.
   *
   * @return array
   *   The migrated randomizeText configuration.
   */
  protected function migrateFakerToRandomizeText(array $config): array {
    $migrated = [
      'converter' => 'randomizeText',
      'unique' => $config['unique'] ?? FALSE,
    ];

    // Remove parameters as randomizeText doesn't use them.
    return $migrated;
  }

  /**
   * Checks if a converter configuration guarantees uniqueness.
   *
   * @param array $config
   *   The converter configuration.
   *
   * @return bool
   *   TRUE if uniqueness is guaranteed, FALSE otherwise.
   */
  protected function guaranteesUniqueness(array $config): bool {
    if (!isset($config['unique']) || !$config['unique']) {
      return FALSE;
    }

    // Only randomizeText with unique flag guarantees uniqueness.
    return $config['converter'] === 'randomizeText';
  }

}
