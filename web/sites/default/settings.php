<?php

// @codingStandardsIgnoreFile

// Database settings.
$databases['default']['default'] = [
  'database' =>  getenv('DB_NAME'),
  'username' => getenv('DB_USER'),
  'password' => getenv('DB_PASS'),
  'host' => getenv('DB_HOST'),
  'port' => '3306',
  'driver' => 'mysql',
  'prefix' => '',
  'collation' => 'utf8mb4_general_ci',
];

if (getenv('LANDO_INFO')) {
  /*
   * Load database credentials from Lando.
   */
  $lando_info = json_decode(getenv('LANDO_INFO'), TRUE);
  $databases['default']['default'] = [
    'driver' => 'mysql',
    'database' => $lando_info['database']['creds']['database'],
    'username' => $lando_info['database']['creds']['user'],
    'password' => $lando_info['database']['creds']['password'],
    'host' => $lando_info['database']['internal_connection']['host'],
    'port' => $lando_info['database']['internal_connection']['port'],
  ];
}

// Location of the site configuration files.
$config_directories[CONFIG_SYNC_DIRECTORY] = '../config/sync';

// Salt for one-time login links, cancel links, form tokens, etc.
$settings['hash_salt'] = getenv('HASH_SALT');

/**
 * Load services definition file.
 */
$settings['container_yamls'][] = $app_root . '/' . $site_path . '/services.yml';

/**
 * The default list of directories that will be ignored by Drupal's file API.
 *
 * By default ignore node_modules folders to avoid issues
 * with common frontend tools and recursive scanning of directories looking for
 * extensions.
 *
 * @see file_scan_directory()
 * @see \Drupal\Core\Extension\ExtensionDiscovery::scanDirectory()
 */
$settings['file_scan_ignore_directories'] = [
  'node_modules',
];

/**
 * Generated twig files should not be on shared storage.
 */
$settings['php_storage']['twig']['directory'] = '/tmp';

/**
 * If a volume has been set for private files, tell Drupal about it.
 */
if (getenv('PRIVATE_FILES_PATH')) {
  $settings['file_private_path'] = getenv('PRIVATE_FILES_PATH');
}

/**
 * Set the memcache server hostname when a memcached server is available
 */
if (getenv('MEMCACHED_HOST')) {
  $settings['memcache']['servers'] = [getenv('MEMCACHED_HOST') . ':11211' => 'default'];
}

/**
 * Load local development override configuration, if available.
 *
 * Use settings.local.php to override variables on secondary (staging,
 * development, etc) installations of this site. Typically used to disable
 * caching, JavaScript/CSS compression, re-routing of outgoing emails, and
 * other things that should not happen on development and testing sites.
 *
 * Keep this code block at the end of this file to take full effect.
 */
if (file_exists($app_root . '/' . $site_path . '/settings.local.php')) {
  include $app_root . '/' . $site_path . '/settings.local.php';
}
