# Documentation

Comprehensive documentation for the Drupal project with Kubernetes deployment.

## Development and testing

### Testing utilities

- [Testing utilities module](../web/modules/custom/testing_utilities/README.md) - Development and testing utilities for data anonymization, validation, and compliance testing

## Implementation guides

### Data privacy and compliance

- [SLT-968: GDPR dump randomizeText implementation](SLT-968-gdpr-dump-randomizetext-implementation.md) - Implementation guide for ensuring unique name generation in gdprDump configuration

## Project structure

### Custom modules

- [testing_utilities](../web/modules/custom/testing_utilities/README.md) - Testing utilities for data anonymization and compliance

### Configuration

- `charts/drupal/values.yaml` - Kubernetes deployment configuration including gdprDump settings
- `phpunit.xml` - PHPUnit test configuration
- `grumphp.yml` - Code quality and testing automation

## Development workflow

### Local development

```bash
# Start development environment
lando start

# Install dependencies
lando composer install

# Enable testing utilities
lando drush en testing_utilities -y

# Run tests
lando grumphp run --tasks=phpunit
```

### Testing data anonymization

```bash
# Generate anonymized database dump
lando gdpr-dump web/modules/custom/testing_utilities/config/gdpr-dump.yaml > /tmp/anonymized-dump.sql

# Validate anonymization
lando phpunit --testsuite=unit web/modules/custom/testing_utilities/
```

## Contributing

Follow the project's coding standards and testing requirements:

1. Run automated tests before committing
2. Update documentation for significant changes
3. Follow [documentation standards](../.ai/rules/docs.md)
4. Test data anonymization changes thoroughly
