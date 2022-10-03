<?php

use Drupal\Core\File\FileSystemInterface;

// Same as error_reporting(E_ALL);
ini_set('error_reporting', E_ALL);

// CHANGE DRUPAL TMP PATH FROM /tmp DUE TO PERMISSION ISSUE
// $settings['file_temp_path'] = $_SERVER['HOME'] . '/tmp';
print ("GET_CURRENT_USER: " . get_current_user() . "\n");
print ("_SERVER['HOME']: " . $_SERVER['HOME'] . "\n");
print ("sys_get_temp_dir() : " . sys_get_temp_dir() . "\n");

$tmppath = \Drupal::service('file_system')->realpath('temporary://');
print ("TMP FOLDER PATH: " . $tmppath . "\n");

// ######## TEST 1
// Check that the files directory is operating properly.
$tmp = \Drupal::service('file_system')->tempnam($path, 'status_check_');
// Use env var for testing this code branch.
// Cannot test it otherwise.
if (empty($tmp)) {
  print("DEFAULT FS NOT WRITABLE" . "\n");
  $status = 'warning';
  $message = "Can't create file.";
  $payload = [
    'file' => $tmp,
  ];
}
else {
  print("DEFAULT FS IS WRITABLE" . "\n");
}

print ("\n-----\n");

// ######## TEST 2
$directory = 'public://test-folder';

$file_system = \Drupal::service('file_system');
$testfolder = $file_system->prepareDirectory($directory, FileSystemInterface:: CREATE_DIRECTORY | FileSystemInterface::MODIFY_PERMISSIONS);
// $file_system->copy($filepath, $directory . '/' . basename($filepath), FileSystemInterface::EXISTS_REPLACE);
print("TESTFOLDER STATUS:" . $testfolder . "\n");

$writable = is_writable($directory);
if (!$writable) {
  print("TESTFOLDER NOT WRITABLE:" . $directory . "\n");
}
else {
  print("TESTFOLDER IS WRITABLE:" . $directory . "\n");
}

print ("\n-----\n");

// ######## TEST 3
// $status = exec('echo 123 >> /silta/tmp/test.txt', $retArr, $retVal);
// echo "--".$status."--";
// print_r($retArr);
// print_r($retVal);
// echo "****";

// if (!$status) {
//   print("TEST 3 FILE NOT WRITABLE" . "\n");
// }
// else {
//   print("TEST 3 FILE IS WRITABLE" . "\n");
// }

// print ("\n-----\n");

// ######## TEST 4
// https://api.drupal.org/api/drupal/core%21lib%21Drupal%21Core%21File%21FileSystem.php/function/FileSystem%3A%3AsaveData/9.4.x
$data = 'Text file example content';
$temp_name = '/silta/tmp/test4.txt';
print ("CREATING FILE with file_put_contents(): " . $temp_name . "\n");
if (file_put_contents($temp_name, $data) === FALSE) {
  print("TEMP FILE NOT WRITABLE" . "\n");
}
else {
  print("TEMP FILE IS WRITABLE" . "\n");
}
print ("\n-----\n");

// ######## TEST 5
// https://api.drupal.org/api/drupal/core%21lib%21Drupal%21Core%21File%21FileSystem.php/function/FileSystem%3A%3AsaveData/9.4.x
$data = 'Text file example content';

$temp_name = \Drupal::service('file_system')->realpath(\Drupal::service('file_system')->tempnam('temporary://', 'file'));
// $temp_name = 'temporary://test5.txt';
print ("CREATING FILE with file_put_contents(): " . $temp_name . "\n");
if (file_put_contents($temp_name, $data) === FALSE) {
  print("TEMP FILE NOT WRITABLE" . "\n");
}
else {
  print("TEMP FILE IS WRITABLE" . "\n");
}
print ("\n-----\n");

// ######## TEST 5
// https://api.drupal.org/api/drupal/core%21lib%21Drupal%21Core%21File%21FileSystem.php/function/FileSystem%3A%3AsaveData/9.4.x
$data = 'Text file example content';

$temp_name = \Drupal::service('file_system')->tempnam('temporary://', 'file');
print ("CREATING FILE with file_put_contents(): " . $temp_name . "\n");
if (file_put_contents($temp_name, $data) === FALSE) {
  print("TEMP FILE NOT WRITABLE" . "\n");
}
else {
  print("TEMP FILE IS WRITABLE" . "\n");
}
print ("\n-----\n");

// ######## TEST 6
// https://api.drupal.org/api/drupal/core%21lib%21Drupal%21Core%21File%21FileSystem.php/function/FileSystem%3A%3AsaveData/9.4.x
$data = 'Text file example content';

$temp_name = \Drupal::service('file_system')->tempnam('private://', 'file');
print ("CREATING FILE with file_put_contents(): " . $temp_name . "\n");
if (file_put_contents($temp_name, $data) === FALSE) {
  print("TEMP FILE NOT WRITABLE" . "\n");
}
else {
  print("TEMP FILE IS WRITABLE" . "\n");
}
print ("\n-----\n");

// ######## TEST 7
// https://api.drupal.org/api/drupal/core%21lib%21Drupal%21Core%21File%21FileSystem.php/function/FileSystem%3A%3AsaveData/9.4.x
$data = 'Text file example content';

$temp_name = \Drupal::service('file_system')->tempnam('public://', 'file');
print ("CREATING FILE with file_put_contents(): " . $temp_name . "\n");
if (file_put_contents($temp_name, $data) === FALSE) {
  print("TEMP FILE NOT WRITABLE" . "\n");
}
else {
  print("TEMP FILE IS WRITABLE" . "\n");
}
print ("\n-----\n");

// ######## TEST 8
$temp_name = '/silta/tmp/test7.txt';
print ("CREATING FILE with fopen() + fwrite(): " . $temp_name . "\n");
$f = fopen($temp_name, 'w');
if (!$f) {
  print("TEMP FILE: CAN'T FOPEN FILE" . "\n");
} else {
  print("TEMP FILE: FOPEN WORKS, WRITING" . "\n");
  $bytes = fwrite($f, $data);
  fclose($f);
  print("TEMP FILE STATUS: " . $bytes . "\n");
}
print ("\n-----\n");

// ######## TEST 9
$temp_name = \Drupal::service('file_system')->tempnam('temporary://', 'file');
print ("CREATING FILE with fopen() + fwrite(): " . $temp_name . "\n");
$f = fopen($temp_name, 'w');
if (!$f) {
  print("TEMP FILE: CAN'T FOPEN FILE" . "\n");
} else {
  print("TEMP FILE: FOPEN WORKS, WRITING" . "\n");
  $bytes = fwrite($f, $data);
  fclose($f);
  print("TEMP FILE STATUS: " . $bytes . "\n");
}
print ("\n-----\n");

// ######## TEST 10
$temp_name = \Drupal::service('file_system')->tempnam('private://', 'file');
print ("CREATING FILE with fopen() + fwrite(): " . $temp_name . "\n");
$f = fopen($temp_name, 'w');
if (!$f) {
  print("TEMP FILE: CAN'T FOPEN FILE" . "\n");
} else {
  print("TEMP FILE: FOPEN WORKS, WRITING" . "\n");
  $bytes = fwrite($f, $data);
  fclose($f);
  print("TEMP FILE STATUS: " . $bytes . "\n");
}
print ("\n-----\n");

// ######## TEST 11
$temp_name = \Drupal::service('file_system')->tempnam('public://', 'file');
print ("CREATING FILE with fopen() + fwrite(): " . $temp_name . "\n");
$f = fopen($temp_name, 'w');
if (!$f) {
  print("TEMP FILE: CAN'T FOPEN FILE" . "\n");
} else {
  print("TEMP FILE: FOPEN WORKS, WRITING" . "\n");
  $bytes = fwrite($f, $data);
  fclose($f);
  print("TEMP FILE STATUS: " . $bytes . "\n");
}
print ("\n-----\n");

// ######## TEST 12
$data = 'Text file example content';

/** @var \Drupal\file\FileRepositoryInterface $fileRepository */
$fileRepository = \Drupal::service('file.repository');
print("TESTFILE PRE:" . "\n");
$testfile = $fileRepository->writeData($data, $directory . "/TEST_FILE.txt", FileSystemInterface::EXISTS_REPLACE);
print("TESTFILE STATUS:" . $testfile . "\n");
print ("\n-----\n");

// 13

// if (isset($destination) && !\Drupal::service('file_system')->prepareDirectory($destination, FileSystemInterface::CREATE_DIRECTORY)) {
//   \Drupal::logger('file')->notice('The upload directory %directory for the file field %name could not be created or is not accessible. A newly uploaded file could not be saved in this directory as a consequence, and the upload was canceled.', ['%directory' => $destination, '%name' => $element['#field_name']]);
//   $form_state->setError($element, t('The file could not be uploaded.'));
//   return FALSE;
// }