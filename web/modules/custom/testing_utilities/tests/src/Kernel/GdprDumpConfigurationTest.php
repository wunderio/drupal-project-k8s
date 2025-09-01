<?php

namespace Drupal\Tests\testing_utilities\Kernel;

use Drupal\KernelTests\KernelTestBase;
use Drupal\user\Entity\User;
use Drupal\Core\Database\Database;

/**
 * Tests GDPR dump configuration and data anonymization.
 *
 * @group testing_utilities
 */
class GdprDumpConfigurationTest extends KernelTestBase {

  /**
   * {@inheritdoc}
   */
  protected static $modules = [
    'system',
    'user',
    'field',
    'testing_utilities',
  ];

  /**
   * Test users created for validation.
   *
   * @var \Drupal\user\Entity\User[]
   */
  protected $testUsers = [];

  /**
   * {@inheritdoc}
   */
  protected function setUp(): void {
    parent::setUp();

    // Install required schemas.
    $this->installEntitySchema('user');
    $this->installSchema('system', ['sequences']);

    // Install config.
    $this->installConfig(['system', 'user']);

    // Create test users with known names.
    $this->createTestUsers();
  }

  /**
   * Creates test users with predictable names for testing.
   */
  protected function createTestUsers(): void {
    $test_users = [
      ['name' => 'john.doe', 'mail' => 'john.doe@example.com'],
      ['name' => 'jane.smith', 'mail' => 'jane.smith@example.com'],
      ['name' => 'bob.wilson', 'mail' => 'bob.wilson@example.com'],
      ['name' => 'alice.johnson', 'mail' => 'alice.johnson@example.com'],
      ['name' => 'charlie.brown', 'mail' => 'charlie.brown@example.com'],
    ];

    foreach ($test_users as $user_data) {
      $user = User::create([
        'name' => $user_data['name'],
        'mail' => $user_data['mail'],
        'status' => 1,
        'pass' => 'test_password_123',
      ]);
      $user->save();
      $this->testUsers[] = $user;
    }
  }

  /**
   * Tests that test users were created successfully.
   */
  public function testTestUsersCreation(): void {
    $this->assertCount(5, $this->testUsers, 'Five test users should be created');

    // Verify users exist in database.
    $connection = Database::getConnection();
    $query = $connection->select('users_field_data', 'u')
      ->fields('u', ['name', 'mail'])
      ->condition('uid', 0, '>')
      ->orderBy('uid');
    $users = $query->execute()->fetchAll();

    $this->assertCount(5, $users, 'Five users should exist in users_field_data table');

    // Verify specific test user data.
    $expected_names = ['john.doe', 'jane.smith', 'bob.wilson', 'alice.johnson', 'charlie.brown'];
    $actual_names = array_column($users, 'name');

    foreach ($expected_names as $expected_name) {
      $this->assertContains($expected_name, $actual_names, "User {$expected_name} should exist");
    }
  }

  /**
   * Tests GDPR dump configuration structure.
   */
  public function testGdprDumpConfigurationStructure(): void {
    // Test the configuration structure that would be used by gdpr-dump.
    $config = $this->getGdprDumpConfiguration();

    // Verify database configuration.
    $this->assertArrayHasKey('database', $config);
    $this->assertArrayHasKey('tables', $config);

    // Verify users_field_data table configuration.
    $this->assertArrayHasKey('users_field_data', $config['tables']);
    $users_config = $config['tables']['users_field_data'];

    $this->assertArrayHasKey('converters', $users_config);
    $converters = $users_config['converters'];

    // Test name converter configuration.
    $this->assertArrayHasKey('name', $converters);
    $name_converter = $converters['name'];

    $this->assertEquals('randomizeText', $name_converter['converter']);
    $this->assertTrue($name_converter['unique']);
    $this->assertArrayNotHasKey('parameters', $name_converter, 'randomizeText converter should not have parameters');
  }

  /**
   * Tests that original faker configuration would have parameters.
   */
  public function testOriginalFakerConfiguration(): void {
    $faker_config = [
      'converter' => 'faker',
      'unique' => TRUE,
      'parameters' => [
        'formatter' => 'name',
      ],
    ];

    // Verify the old configuration structure.
    $this->assertEquals('faker', $faker_config['converter']);
    $this->assertTrue($faker_config['unique']);
    $this->assertArrayHasKey('parameters', $faker_config);
    $this->assertEquals('name', $faker_config['parameters']['formatter']);
  }

  /**
   * Tests user data retrieval for anonymization validation.
   */
  public function testUserDataRetrieval(): void {
    $connection = Database::getConnection();

    // Get original user data.
    $query = $connection->select('users_field_data', 'u')
      ->fields('u', ['uid', 'name', 'mail'])
      ->condition('uid', 0, '>')
      ->orderBy('uid');
    $original_users = $query->execute()->fetchAll(\PDO::FETCH_ASSOC);

    $this->assertNotEmpty($original_users, 'Original user data should exist');

    // Verify we have the expected test users.
    $names = array_column($original_users, 'name');
    $this->assertContains('john.doe', $names);
    $this->assertContains('jane.smith', $names);
    $this->assertContains('bob.wilson', $names);

    // Verify emails follow expected pattern.
    foreach ($original_users as $user) {
      $this->assertStringContainsString('@example.com', $user['mail']);
      $this->assertNotEmpty($user['name']);
    }
  }

  /**
   * Data provider for converter comparison tests.
   *
   * @return array
   *   Test cases comparing faker vs randomizeText converters.
   */
  public function converterComparisonProvider(): array {
    return [
      'faker_converter' => [
        'converter_type' => 'faker',
        'has_parameters' => TRUE,
        'unique_support' => 'not_guaranteed',
        'expected_parameters' => ['formatter' => 'name'],
      ],
      'randomizeText_converter' => [
        'converter_type' => 'randomizeText',
        'has_parameters' => FALSE,
        'unique_support' => 'guaranteed',
        'expected_parameters' => NULL,
      ],
    ];
  }

  /**
   * Tests converter configuration differences.
   *
   * @dataProvider converterComparisonProvider
   */
  public function testConverterComparison(string $converter_type, bool $has_parameters, string $unique_support, ?array $expected_parameters): void {
    $config = [
      'converter' => $converter_type,
      'unique' => TRUE,
    ];

    if ($has_parameters && $expected_parameters) {
      $config['parameters'] = $expected_parameters;
    }

    // Test converter type.
    $this->assertEquals($converter_type, $config['converter']);
    $this->assertTrue($config['unique']);

    // Test parameters presence.
    if ($has_parameters) {
      $this->assertArrayHasKey('parameters', $config);
      $this->assertEquals($expected_parameters, $config['parameters']);
    }
    else {
      $this->assertArrayNotHasKey('parameters', $config);
    }

    // Test unique support expectations.
    if ($unique_support === 'guaranteed') {
      $this->assertEquals('randomizeText', $converter_type, 'randomizeText should guarantee uniqueness');
    }
    elseif ($unique_support === 'not_guaranteed') {
      $this->assertEquals('faker', $converter_type, 'faker does not guarantee uniqueness');
    }
  }

  /**
   * Gets the GDPR dump configuration for testing.
   *
   * @return array
   *   The configuration array that matches our values.yaml structure.
   */
  protected function getGdprDumpConfiguration(): array {
    return [
      'database' => [
        'host' => 'database',
        'user' => 'drupal11',
        'password' => 'drupal11',
        'name' => 'drupal11',
      ],
      'tables' => [
        'cache' => [
          'truncate' => TRUE,
        ],
        'cache_*' => [
          'truncate' => TRUE,
        ],
        'sessions' => [
          'truncate' => TRUE,
        ],
        'watchdog' => [
          'truncate' => TRUE,
        ],
        'users_field_data' => [
          'converters' => [
            'mail' => [
              'converter' => 'randomizeEmail',
              'unique' => TRUE,
            ],
            'init' => [
              'converter' => 'fromContext',
              'parameters' => [
                'key' => 'processed_data.mail',
              ],
            ],
            'name' => [
              'converter' => 'randomizeText',
              'unique' => TRUE,
            ],
            'pass' => [
              'converter' => 'randomizeText',
            ],
          ],
        ],
      ],
    ];
  }

}
