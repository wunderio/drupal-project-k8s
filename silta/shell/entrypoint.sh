#!/bin/bash

# Generate new SSH fingerprint
ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
ssh-keygen -f /etc/ssh/ssh_host_ecdsa_key -N '' -t ecdsa
ssh-keygen -f /etc/ssh/ssh_host_ed25519_key -N '' -t ed25519

# SSHD settings
sed -i 's/^PasswordAuthentication .*/PasswordAuthentication no/' /etc/ssh/sshd_config
sed -i 's/^#UseDNS .*/UseDNS no/' /etc/ssh/sshd_config
sed -i 's/^#PrintMotd .*/PrintMotd no/' /etc/ssh/sshd_config
sed -i 's/^#PermitUserEnvironment .*/PermitUserEnvironment yes/' /etc/ssh/sshd_config
sed -i 's/^#ChallengeResponseAuthentication .*/ChallengeResponseAuthentication no/' /etc/ssh/sshd_config

# Check the $GITAUTH_REPOSITORY_URL variable
if [[ -v GITAUTH_REPOSITORY_URL ]]; then
    # Extract auth repo url to variables
    RE="^(https|git)(:\/\/|@)([^\/:]+)[\/:]([^\/:]+)\/(.+).git$"
    if [[ $GITAUTH_REPOSITORY_URL =~ $RE ]]; then
        GITAUTH_HOST=${BASH_REMATCH[3]}
        GITAUTH_ORGANISATION=${BASH_REMATCH[4]}
        GITAUTH_PROJECT=${BASH_REMATCH[5]}
    fi
fi

if [[ -v GITAUTH_ORGANISATION && -v GITAUTH_API_TOKEN && -v GITAUTH_HOST ]]; then
    # Use gitauth script only when we have at least the token, host and organisation variables defined
    sed -i 's/^#AuthorizedKeysCommandUser .*/AuthorizedKeysCommandUser nobody/' /etc/ssh/sshd_config
    sed -i 's/^#AuthorizedKeysCommand .*/AuthorizedKeysCommand \/etc\/ssh\/gitauth_keys.py/' /etc/ssh/sshd_config

    # AuthorizedKeysCommand does not pass environment variables so we're storing them in config file
    echo "gitauth-api-token: ${GITAUTH_API_TOKEN}" >> /etc/ssh/gitauth_keys.yaml
    echo "gitauth-host: ${GITAUTH_HOST}" >> /etc/ssh/gitauth_keys.yaml
    echo "gitauth-organisation: ${GITAUTH_ORGANISATION}" >> /etc/ssh/gitauth_keys.yaml
    echo "gitauth-project: ${GITAUTH_PROJECT}" >> /etc/ssh/gitauth_keys.yaml
fi

env > /etc/environment
addgroup www-admin
# We add -D to make it non-interactive, but then the user is locked out.
adduser www-admin -D -G www-admin -s /bin/bash -h /var/www/html
# So set an empty password after the user is created.
echo "www-admin:" | chpasswd

# Pass environment variables down to container, so SSH can pick it up and drush commands work too.
mkdir ~www-admin/.ssh/
env | grep -v HOME > ~www-admin/.ssh/environment

# Put backup key
# mkdir -p /root/.ssh
# touch /root/.ssh/authorized_keys
# echo "pubkey" > /root/.ssh/authorized_keys
# chmod 600 /root/.ssh/authorized_keys

# run SSH server
/usr/sbin/sshd -D -E /proc/self/fd/2
