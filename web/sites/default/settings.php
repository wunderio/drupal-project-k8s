<?php


// CHANGE THIS.
// $settings['hash_salt'] = 'abc';

// if ((isset($_SERVER["HTTPS"]) && strtolower($_SERVER["HTTPS"]) == "on")
//   || (isset($_SERVER["HTTP_X_FORWARDED_PROTO"]) && $_SERVER["HTTP_X_FORWARDED_PROTO"] == "https")
//   || (isset($_SERVER["HTTP_HTTPS"]) && $_SERVER["HTTP_HTTPS"] == "on")
// ) {
//   $_SERVER["HTTPS"] = "on";

//   // Tell Drupal we're using HTTPS (url() for one depends on this).
//   $settings['https'] = TRUE;
// }

// @codingStandardsIgnoreStart
if (isset($_SERVER['REMOTE_ADDR'])) {
  $settings['reverse_proxy'] = TRUE;
  $settings['reverse_proxy_addresses'] = [$_SERVER['REMOTE_ADDR']];
}
// @codingStandardsIgnoreEnd

if (!empty($_SERVER['SERVER_ADDR'])) {
  // This should return last section of IP, such as "198". (dont want/need to expose more info).
  //drupal_add_http_header('X-Webserver', end(explode('.', $_SERVER['SERVER_ADDR'])));
  $pcs = explode('.', $_SERVER['SERVER_ADDR']);
  header('X-Webserver: ' . end($pcs));
}

// If ENVIRONMENT_NAME is set then it's a Silta environment.
if ($environment = getenv('ENVIRONMENT_NAME')) {
  switch ($environment) {
    case 'production':
      $settings['simple_environment_indicator'] = '#d4000f Production';
      break;

    case 'master':
      $settings['simple_environment_indicator'] = '#e56716 Stage';
      break;

    default:
      $settings['simple_environment_indicator'] = '#004984 Development';
      break;
  }
}
else {
  $settings['simple_environment_indicator'] = '#88b700 Local';
}
/**
 * Location of the site configuration files.
 */
$settings['config_sync_directory'] = '../sync';

/**
 * Temporary files directory path.
 */
$settings['file_temp_path'] = '/tmp';

/**
 * Access control for update.php script.
 */
$settings['update_free_access'] = FALSE;

/**
 * The default list of directories that will be ignored by Drupal's file API.
 *
 * By default ignore node_modules and bower_components folders to avoid issues
 * with common frontend tools and recursive scanning of directories looking for
 * extensions.
 *
 * @see file_scan_directory()
 * @see \Drupal\Core\Extension\ExtensionDiscovery::scanDirectory()
 */
$settings['file_scan_ignore_directories'] = [
  'node_modules',
  'bower_components',
];

/**
 * Exclude development modules from configuration synchronisation.
 */
$settings['config_exclude_modules'] = ['devel', 'stage_file_proxy', 'update'];

// -----------------

// Location of the site configuration files.
$settings['config_sync_directory'] = '../config/sync';

/**
 * Load services definition file.
 */
$settings['container_yamls'][] = $app_root . '/' . $site_path . '/services.yml';

/**
 * The default list of directories that will be ignored by Drupal's file API.
 *
 * By default ignore node_modules and bower_components folders to avoid issues
 * with common frontend tools and recursive scanning of directories looking for
 * extensions.
 *
 * @see file_scan_directory()
 * @see \Drupal\Core\Extension\ExtensionDiscovery::scanDirectory()
 */
$settings['file_scan_ignore_directories'] = [
  'node_modules',
  'bower_components',
];

/**
 * Load local development override configuration, if available.
 *
 * Use settings.local.php to override variables on secondary (staging,
 * development, etc) installations of this site. Typically used to disable
 * caching, JavaScript/CSS compression, re-routing of outgoing emails, and
 * other things that should not happen on development and testing sites.
 */
if (file_exists($app_root . '/' . $site_path . '/settings.local.php')) {
  include $app_root . '/' . $site_path . '/settings.local.php';
}

/**
 * Lando configuration overrides.
 */
if (getenv('LANDO_INFO') && file_exists($app_root . '/' . $site_path . '/settings.lando.php')) {
  include $app_root . '/' . $site_path . '/settings.lando.php';
}

/**
 * Silta cluster configuration overrides.
 */
if (getenv('SILTA_CLUSTER') && file_exists($app_root . '/' . $site_path . '/settings.silta.php')) {
  include $app_root . '/' . $site_path . '/settings.silta.php';
}
