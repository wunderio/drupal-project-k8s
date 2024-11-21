# Drupal AI Experiments

This project is the home for all Drupal related artificial intelligence experiments.

## Main environment

- URL: <https://main.internal-drupal-ai.dev.wdr.io>
- Drush alias: `drush @main st`
- SSH: `ssh www-admin@main-shell.internal-drupal-ai -J www-admin@ssh.dev.wdr.io`

Drush alias for the **current** Silta feature branch deployment is `drush @current st`.

## DDEV local environment

1. Install DDEV and its dependencies (https://ddev.com/get-started/)
1. Copy `.env.example` to `.ddev/.env`, get the real values from the `silta/secrets` by decrypting it with Silta CLI (see "Secrets handling" below)
1. Run `ddev start` to start the local environment
1. Run `ddev composer install` to install packages
1. Run `ddev import-db --file=mydb.sql.gz` to import the database or install the site from scratch: `ddev drush si --existing-config`

## Lando local environment

- Appserver: <https://internal-drupal-ai.lndo.site>
- Adminer: <http://adminer.internal-drupal-ai.lndo.site>
- Elasticsearch: <http://localhost:9200>, <http://elasticsearch.lndo.site>
- Kibana: <http://localhost:5601>, <http://kibana.lndo.site>
- Mailhog: <http://mail.lndo.site>
- Varnish: <https://varnish.internal-drupal-ai.lndo.site>
- Drush alias: `lando drush @local st`
- SSH: `lando ssh (-s <service>)`

### [Services](https://docs.lando.dev/core/v3/services.html)

- `adminer` - uses [Adminer database management tool](https://github.com/dehy/docker-adminer).
- `chrome` - uses [selenium/standalone-chrome](https://hub.docker.com/r/selenium/standalone-chrome/) image, uncomment the service definition at `.lando.yml` to enable.
- `elasticsearch` - uses official [Elasticsearch image](https://hub.docker.com/r/elastic/elasticsearch), uncomment the service definition at `.lando.yml` to enable. Requires [at least 4GiB of memory](https://www.elastic.co/guide/en/elasticsearch/reference/current/docker.html).
- `kibana` - uses official [Kibana image](https://hub.docker.com/r/elastic/kibana), uncomment the service definition at `.lando.yml` to enable.
- `mailhog` - uses Lando [MailHog service](https://docs.lando.dev/mailhog/).
- `node` - uses Lando [Node service](https://docs.lando.dev/node/).
- `varnish` - uses Lando [Varnish service](https://docs.lando.dev/varnish/), uncomment the service definition at `.lando.yml` to enable.

### [Tools](https://docs.lando.dev/core/v3/tooling.html)

- `lando` - tools / commands overview.
- `lando grumphp <commands>` - run [GrumPHP](https://github.com/phpro/grumphp) code quality checks. Modified or new files are checked on git commit, see more at `lando grumphp -h` or [wunderio/code-quality](https://github.com/wunderio/code-quality).
- `lando npm <commands>` - run [npm](https://www.npmjs.com/) commands.
- `lando phpunit <commands>` - run [PHPUnit](https://phpunit.de/) commands.
- `lando varnishadm <commands>` - run [varnishadm](https://varnish-cache.org/docs/6.0/reference/varnishadm.html) commands.
- `lando xdebug <mode>` - load [Xdebug](https://xdebug.org/) in the selected [mode(s)](https://xdebug.org/docs/all_settings#mode).

## OpenAI API credentials in local

Make sure to check the .env.example to get local working with OpenAI API.

## Secrets handling

[Silta CLI](https://github.com/wunderio/silta-cli) is a command-line tool that helps you manage secrets and configurations for your Silta projects. You can use Silta CLI to encrypt and decrypt files with the following commands:

- `silta secrets encrypt --file silta/secrets --secret-key=$SECRET_KEY` to encrypt a file.
- `silta secrets decrypt --file silta/secrets --secret-key=$SECRET_KEY` to decrypt a file.
- `silta secrets --help` for help.

To use these commands, you need a secret key that is used to encrypt and decrypt the data. It can be specified with the `--secret-key` flag (defaults to the `SECRET_KEY` environment variable which can be found from LastPass: "silta-dev SECRET_KEY"). It is strongly advised to use a custom key for each project to ensure security and avoid conflicts. See Silta's documentation on [how to use a custom decryption key in a CircleCI configuration](https://wunderio.github.io/silta/docs/encrypting-sensitive-configuration/#using-a-custom-encryption-key).

## Running LLM locally

To run an open source LLM locally, follow the steps below:

1. Download LM Studio from https://lmstudio.ai/
1. After installing the LM Studio, download the model of your preference from the LM Studio.
1. Go to the Developer section of the LM Studio and start a server. Make sure to choose the "Serve on local network" option.
1. Check the "Server logs" to get the address to the server.
1. Navigate to https://drupal-k8s.ddev.site/admin/config/ai/providers/lmstudio and provide the hostname and port.
1. Go to https://drupal-k8s.ddev.site/admin/config/ai/explorers/chat-generation and try out the LLM.
1. You are ready to use the LLM locally and build whatever you need in the local environment.
