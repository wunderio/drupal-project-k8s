# Testing Utilities Module

A comprehensive development and testing utilities module for data anonymization, validation, and compliance testing.

## Purpose

This module provides extensive testing coverage and utilities for:

- Data anonymization testing (GDPR dump, etc.)
- Configuration validation and testing
- Database operations testing
- Privacy compliance testing
- Development debugging utilities
- Mock data generation and validation

## Features

### Test coverage

- **Unit Tests**: Configuration validation and converter logic (9 tests, 28 assertions)
- **Kernel Tests**: Drupal integration and database operations (6 tests, 60 assertions)
- **Functional Tests**: Full application behavior testing (5 tests, 15 assertions)

### Configuration testing

- Validates `randomizeText` vs `faker` converter behavior
- Tests uniqueness guarantees for anonymized data
- Verifies configuration structure and validation rules

### Real-world validation

- Tests actual gdpr-dump binary execution (when available)
- Validates anonymization effectiveness
- Ensures compliance with SLT-968 requirements

## Installation

Enable the module for testing purposes:

```bash
lando drush en testing_utilities -y
```

## Running tests

### All tests

```bash
lando grumphp run --tasks=phpunit
```

### Specific test suites

```bash
# Unit tests
lando phpunit --testsuite=unit web/modules/custom/testing_utilities/

# Kernel tests
lando phpunit --testsuite=kernel web/modules/custom/testing_utilities/

# Functional tests
lando phpunit --testsuite=functional web/modules/custom/testing_utilities/
```

### Individual test classes

```bash
lando phpunit web/modules/custom/testing_utilities/tests/src/Unit/GdprDumpConfigValidatorTest.php
lando phpunit web/modules/custom/testing_utilities/tests/src/Kernel/GdprDumpConfigurationTest.php
lando phpunit web/modules/custom/testing_utilities/tests/src/Functional/GdprDumpExecutionTest.php
```

## Configuration Files

### config/gdpr-dump.yaml

Local development configuration that mirrors production settings from `charts/drupal/values.yaml`.

**Usage:**

```bash
lando gdpr-dump web/modules/custom/testing_utilities/config/gdpr-dump.yaml > /tmp/anonymized-dump.sql
```

## SLT-968 implementation

This module validates the SLT-968 implementation which changed the user name converter from `faker` to `randomizeText` to ensure unique name generation.

### Key changes tested

- `faker` converter with `unique: true` does not guarantee uniqueness
- `randomizeText` converter with `unique: true` guarantees uniqueness
- Configuration simplification (no parameters needed for `randomizeText`)

## Dependencies

- Drupal 11
- PHPUnit 10.5+
- gdpr-dump binary (for real-world testing)

## Future expansion

This module is designed to be extensible for various testing utilities:

### Potential additions

- **Performance testing utilities**: Database query performance, load testing helpers
- **Integration testing helpers**: API testing, external service mocking
- **Data validation utilities**: Schema validation, data integrity checks
- **Development debugging tools**: Query logging, performance profiling
- **Mock data generators**: Test data factories, fixture generators
- **Security testing utilities**: Input validation, access control testing

### Contributing

When adding new testing utilities:

1. Follow the existing test structure (Unit/Kernel/Functional)
2. Add comprehensive documentation
3. Include configuration examples in the `config/` directory
4. Update this README with new functionality
