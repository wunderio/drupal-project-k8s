#!/bin/sh

# Debugging
env
touch /testfile
env > /testfile2

# Generate new SSH fingerprint
ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
ssh-keygen -f /etc/ssh/ssh_host_ecdsa_key -N '' -t ecdsa
ssh-keygen -f /etc/ssh/ssh_host_ed25519_key -N '' -t ed25519

# Extend authorization keys with our script
sed -i 's/^PasswordAuthentication .*/PasswordAuthentication no/' /etc/ssh/sshd_config
sed -i 's/^#AuthorizedKeysCommandUser .*/AuthorizedKeysCommandUser nobody/' /etc/ssh/sshd_config
sed -i 's/^#AuthorizedKeysCommand .*/AuthorizedKeysCommand \/etc\/ssh\/authorized_keys.py/' /etc/ssh/sshd_config
sed -i 's/^#UseDNS .*/UseDNS no/' /etc/ssh/sshd_config

# AuthorizedKeysCommand does not pass environment variables so we're storing them in config file
sed -i "s/GITHUB_API_ACCESS_TOKEN/${GIT_AUTH_TOKEN}/g" /etc/ssh/authorized_keys.yaml
#echo "github-api-access-token: ${GIT_AUTH_TOKEN}" >> /etc/ssh/authorized_keys.yaml


# Put backup key
mkdir -p /root/.ssh
touch /root/.ssh/authorized_keys
echo "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCSv9nS5EEw6SkT5QOfAdC30lVKT9h+ogA5MhVHqbjU6kLXEPLQkgIJ5EozS4syhDCCl5t758Y3qi4RllOXILwQuwnEZzOz7R6VGuJwY+x/ftb1HrQUHya8EWQGSLczyJL7daxciwx/SEFsuzlXhU4+1b6hUJK8HyC34cp9M6YrIVbMlP2nxjwMAKaUoq8qYrHKDS7VrcRzMgteLnoJU0M3CNdrrXPkGmHn98zluzcGl5goLeCaXqhSWdQ0vHCN25dautb/ce8zMGhJ0tjtHvgdIbZrXNkGnzLbyRDO2Tif3yfdF5tV8PMQE0kjvtxRFgsS7ZofxmGrHkI+TwcxJhiF janis@test" > /root/.ssh/authorized_keys
chmod 600 /root/.ssh/authorized_keys

# run SSH server
/usr/sbin/sshd -D

