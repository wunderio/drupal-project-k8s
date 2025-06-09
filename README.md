# Drupal Project with DDoS Mitigation Training Environment

This project includes a DDoS mitigation training environment for testing and learning purposes.

## DDoS Mitigation Training Setup

### Prerequisites
- Access to the Kubernetes cluster
- `kubectl` configured with appropriate permissions

### Initial Setup

1. **Ensure Fastly blackhole service is active**
   - Visit https://blackhole.fastly.wdr.io
   - If the site shows a maintenance page, click the "Launch" button
   - Wait about a minute for the service to become active

2. **Adjust PHP container CPU requests**
   ```bash
   kubectl patch deployment ddos-mitigation-drupal -n drupal-project-k8s -p '{"spec":{"template":{"spec":{"containers":[{"name":"php","resources":{"requests":{"cpu":"500m"}}}]}}}}'
   ```
   *Note: This adjustment prevents the autoscaler from overreacting during training.*

### Running the Training

1. **Deploy the attack pod**
   ```bash
   kubectl apply -f attack-pod.yml
   ```

2. **Monitor the attack**
   - Use Kubernetes dashboard or CLI tools to monitor the impact
   - Observe how the system responds to the simulated DDoS attack

### Cleanup

When training is complete, remove the attack pod:
```bash
kubectl delete pod ddos-training-attacker -n drupal-project-k8s
```

---

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

# Secrets

Project can override values and do file encryption using openssl.
Encryption key has to be identical to the one in circleci context.

Decrypting secrets file:
```
openssl enc -d -aes-256-cbc -pbkdf2 -in silta/secrets -out silta/secrets.dec
```

Encrypting secrets file:
```
openssl aes-256-cbc -pbkdf2 -in silta/secrets.dec -out silta/secrets
```

Secret values can be attached to circleci `drupal-build-deploy` job like this
```
decrypt_files: silta/secrets
silta_config: silta/silta.yml,silta/secrets
```
