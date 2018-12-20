# Load testing for Silta

We have a few jmeter configurations for load testing silta. 
In addition to the usual generation of HTTP requests, these configurations
also call shell scripts in this folder to create and delete Helm releases.

Install jmeter via homebrew or your favorite package manager, then run:

```
jmeter -t devtools/basicTest.jmx
```

## basicTest.jmx

This test creates a test release, sends traffic to it, then deletes the release.
This test is useful for testing the performance of a single deployed instance.

## parallelDeploymentTest.jmx

This tests creates multiple test releases in parallel, checks that the release 
works, then deletes them. This test is useful for testing the performance of the 
deployment process, and to reproduce errors that don't happen consistently.