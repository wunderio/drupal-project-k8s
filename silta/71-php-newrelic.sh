#!/bin/bash

# enable newrelic only if NEWRELIC_ENABLED is set
if [ ${NEWRELIC_ENABLED+x} ]; then
  
  # Override newrelic appname with our custom values 
  sed -i -e "s/newrelic.appname = .*/newrelic.appname = \"\${PROJECT_NAME:-noproject}-\${ENVIRONMENT_NAME:-nobranch}\"/" /usr/local/etc/php/conf.d/newrelic.disable

  # envplate the newrelic ini file
  ep /usr/local/etc/php/conf.d/newrelic.disable

  cp /usr/local/etc/php/conf.d/newrelic.disable /usr/local/etc/php/conf.d/newrelic.ini
fi