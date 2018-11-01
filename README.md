# Wunder template for Drupal projects

[![CircleCI](https://circleci.com/gh/wunderio/drupal-project/tree/master.svg?style=svg)](https://circleci.com/gh/wunderio/drupal-project/tree/master)

This project template is an opinionated fork of the popular [Drupal-composer template](https://github.com/drupal-composer/drupal-project), configured to automatically deploy code to a [Kubernetes](https://kubernetes.io/) cluster using [CircleCI](https://circleci.com/). Everything that works with the Drupal-composer project template will work with this repository, so we won't duplicate the documentation here.

## Usage

- Copy this repository and push it to our organization. 
- Log in to CircleCI using your Github account and add the new project.
- Create and maintain a Personal Data mapping list for automatic data sanitization in `gdpr.json` file. See GDPR sanitization section for more information.
 
## How it works

Each pushed commit is processed according to the instructions in `.circleci/config.yml` in the repository. 
Have a look at the file for details, but in short this is how it works:

- Run the CircleCI jobs using a [custom docker image](https://github.com/wunderio/circleci-builder) that has all the tools we need.  
- Validate the codebase with phpcs and other static code analysis tools.
- Build the codebase by downloading vendor code using `composer` and `yarn`.
- Create a custom docker image for Drupal and nginx, and push those to a docker registry (typically that of your cloud provider).
- Install or update our helm chart while passing our custom images as parameters.
- The helm chart executes the usual drush deployment commands.

## GDPR sanitization

SQL data dump for developers is parsed with [GDPR Dump](https://github.com/machbarmacher/gdpr-dump) project.
You can create a `/gdpr.json` file with [Faker](https://packagist.org/packages/fzaninotto/faker) formatters that will allow replacing data as it's dumped from database using `mysqldump` / `drush sql-dump` command.  

```
{
  "users_field_data": {
    "name": {"formatter": "name"},
    "pass": {"formatter": "password"},
    "mail": {"formatter": "email"},
    "init": {"formatter": "clear"}
  }
} 
```
Available formatters listed in [GDPR Dump project documentation](https://github.com/machbarmacher/gdpr-dump#using-gdpr-replacements). 

You can also add extra elements and attributes, like `_cookies`, `_description` or `_purpose` to enrich the Personal Data information. Just make sure it's marked or prefixed so that it does not mess up GDPR dump when it looks for table data replacements.