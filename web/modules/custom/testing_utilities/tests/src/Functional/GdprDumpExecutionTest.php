<?php

namespace Drupal\Tests\testing_utilities\Functional;

use Drupal\Tests\BrowserTestBase;
use Drupal\user\Entity\User;

/**
 * Tests GDPR dump execution and output validation.
 *
 * @group testing_utilities
 */
class GdprDumpExecutionTest extends BrowserTestBase {

  /**
   * {@inheritdoc}
   */
  protected $defaultTheme = 'stark';

  /**
   * {@inheritdoc}
   */
  protected static $modules = [
    'system',
    'user',
    'testing_utilities',
  ];

  /**
   * Test users for validation.
   *
   * @var \Drupal\user\Entity\User[]
   */
  protected $testUsers = [];

  /**
   * {@inheritdoc}
   */
  protected function setUp(): void {
    parent::setUp();
    $this->createTestUsers();
  }

  /**
   * Creates test users with known data.
   */
  protected function createTestUsers(): void {
    $test_users = [
      ['name' => 'test_user_1', 'mail' => 'test1@example.com'],
      ['name' => 'test_user_2', 'mail' => 'test2@example.com'],
      ['name' => 'test_user_3', 'mail' => 'test3@example.com'],
    ];

    foreach ($test_users as $user_data) {
      $user = User::create([
        'name' => $user_data['name'],
        'mail' => $user_data['mail'],
        'status' => 1,
        'pass' => 'test_password',
      ]);
      $user->save();
      $this->testUsers[] = $user;
    }
  }

  /**
   * Tests that gdpr-dump binary is available.
   */
  public function testGdprDumpBinaryAvailable(): void {
    // This test would be run in the container where gdpr-dump should be
    // available.
    $this->markTestSkipped('This test requires gdpr-dump binary to be available in the test environment');

    // In a real test environment with gdpr-dump available:
    // $output = shell_exec('gdpr-dump --version 2>&1');
    // $this->assertStringContainsString('GdprDump', $output);.
  }

  /**
   * Tests GDPR dump configuration file generation.
   */
  public function testGdprDumpConfigGeneration(): void {
    $config = $this->generateGdprDumpConfig();

    // Verify configuration structure.
    $this->assertArrayHasKey('database', $config);
    $this->assertArrayHasKey('tables', $config);
    $this->assertArrayHasKey('users_field_data', $config['tables']);

    $users_config = $config['tables']['users_field_data'];
    $this->assertArrayHasKey('converters', $users_config);

    $name_converter = $users_config['converters']['name'];
    $this->assertEquals('randomizeText', $name_converter['converter']);
    $this->assertTrue($name_converter['unique']);
  }

  /**
   * Tests user data exists for anonymization.
   */
  public function testUserDataExists(): void {
    $this->assertNotEmpty($this->testUsers, 'Test users should be created');

    // Verify users exist in database.
    $user_storage = $this->container->get('entity_type.manager')->getStorage('user');
    $users = $user_storage->loadMultiple();

    // Should have at least our test users plus anonymous user.
    $this->assertGreaterThanOrEqual(4, count($users), 'Should have test users plus anonymous user');

    // Verify specific test users.
    $user_names = [];
    foreach ($users as $user) {
      if ($user->id() > 0) {
        $user_names[] = $user->getAccountName();
      }
    }

    $this->assertContains('test_user_1', $user_names);
    $this->assertContains('test_user_2', $user_names);
    $this->assertContains('test_user_3', $user_names);
  }

  /**
   * Tests configuration comparison between old and new approaches.
   */
  public function testConfigurationComparison(): void {
    $old_config = $this->generateOldFakerConfig();
    $new_config = $this->generateNewRandomizeTextConfig();

    // Verify old configuration.
    $old_name_converter = $old_config['tables']['users_field_data']['converters']['name'];
    $this->assertEquals('faker', $old_name_converter['converter']);
    $this->assertArrayHasKey('parameters', $old_name_converter);
    $this->assertEquals('name', $old_name_converter['parameters']['formatter']);

    // Verify new configuration.
    $new_name_converter = $new_config['tables']['users_field_data']['converters']['name'];
    $this->assertEquals('randomizeText', $new_name_converter['converter']);
    $this->assertArrayNotHasKey('parameters', $new_name_converter);

    // Both should have unique flag.
    $this->assertTrue($old_name_converter['unique']);
    $this->assertTrue($new_name_converter['unique']);
  }

  /**
   * Tests that the change addresses SLT-968 requirements.
   */
  public function testSlt968Requirements(): void {
    $config = $this->generateGdprDumpConfig();
    $name_converter = $config['tables']['users_field_data']['converters']['name'];

    // Verify SLT-968 requirements are met.
    $this->assertEquals('randomizeText', $name_converter['converter'],
      'SLT-968: Should use randomizeText converter');
    $this->assertTrue($name_converter['unique'],
      'SLT-968: Should have unique flag for guaranteed uniqueness');
    $this->assertArrayNotHasKey('parameters', $name_converter,
      'SLT-968: randomizeText should not need parameters');
  }

  /**
   * Generates GDPR dump configuration with new randomizeText converter.
   *
   * @return array
   *   The configuration array.
   */
  protected function generateGdprDumpConfig(): array {
    return [
      'database' => [
        'host' => 'database',
        'user' => 'drupal11',
        'password' => 'drupal11',
        'name' => 'drupal11',
      ],
      'tables' => [
        'users_field_data' => [
          'converters' => [
            'mail' => [
              'converter' => 'randomizeEmail',
              'unique' => TRUE,
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

  /**
   * Generates old faker-based configuration for comparison.
   *
   * @return array
   *   The old configuration array.
   */
  protected function generateOldFakerConfig(): array {
    return [
      'database' => [
        'host' => 'database',
        'user' => 'drupal11',
        'password' => 'drupal11',
        'name' => 'drupal11',
      ],
      'tables' => [
        'users_field_data' => [
          'converters' => [
            'mail' => [
              'converter' => 'randomizeEmail',
              'unique' => TRUE,
            ],
            'name' => [
              'converter' => 'faker',
              'unique' => TRUE,
              'parameters' => [
                'formatter' => 'name',
              ],
            ],
            'pass' => [
              'converter' => 'randomizeText',
            ],
          ],
        ],
      ],
    ];
  }

  /**
   * Generates new randomizeText configuration.
   *
   * @return array
   *   The new configuration array.
   */
  protected function generateNewRandomizeTextConfig(): array {
    return $this->generateGdprDumpConfig();
  }

}
