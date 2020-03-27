{{- define "drupal.release_labels" -}}
app: {{ .Values.app | quote }}
release: {{ .Release.Name }}
{{- end }}

{{- define "drupal.php-container" -}}
image: {{ .Values.php.image | quote }}
env: {{ include "drupal.env" . }}
ports:
  - containerPort: 9000
    name: drupal
{{- end }}

{{- define "drupal.volumeMounts" -}}
{{- range $index, $mount := $.Values.mounts }}
{{- if eq $mount.enabled true }}
- name: drupal-{{ $index }}
  mountPath: {{ $mount.mountPath }}
{{- end }}
{{- end }}
- name: config
  mountPath: /usr/local/etc/php/conf.d/silta.ini
  readOnly: true
  subPath: php_ini
- name: config
  mountPath: /app/web/sites/default/settings.silta.php
  readOnly: true
  subPath: settings_silta_php
- name: config
  mountPath: /app/web/sites/default/silta.services.yml
  readOnly: true
  subPath: silta_services_yml
{{- end }}

{{- define "drupal.volumes" -}}
{{- range $index, $mount := $.Values.mounts }}
{{- if eq $mount.enabled true }}
- name: drupal-{{ $index }}
  persistentVolumeClaim:
    claimName: {{ $.Release.Name }}-{{ $index }}
{{- end }}
{{- end }}
- name: config
  configMap:
    name: {{ .Release.Name }}-drupal
{{- end }}

{{- define "drupal.imagePullSecrets" }}
{{- if .Values.imagePullSecrets }}
imagePullSecrets:
{{ .Values.imagePullSecrets | toYaml }}
{{- end }}
{{- end }}

{{- define "smtp.env" }}
- name: SMTP_ADDRESS
  {{- if .Values.mailhog.enabled }}
  value: "{{ .Release.Name }}-mailhog:1025"
  {{ else }}
  value: {{ .Values.smtp.address | quote }}
  {{- end }}
- name: SMTP_TLS
  value: {{ .Values.smtp.tls | default false | quote }}
- name: SMTP_STARTTLS
  value: {{ .Values.smtp.starttls | default false | quote }}
- name: SMTP_USERNAME
  value: {{ .Values.smtp.username | quote }}
- name: SMTP_PASSWORD
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-smtp
      key: password
# Duplicate SMTP env variables for ssmtp bundled with amazee php image 
- name: SSMTP_MAILHUB
  {{- if .Values.mailhog.enabled }}
  value: "{{ .Release.Name }}-mailhog:1025"
  {{ else }}
  value: {{ .Values.smtp.address | quote }}
  {{- end }}
- name: SSMTP_USETLS
  value: {{ .Values.smtp.tls | default false | quote }}
- name: SSMTP_USESTARTTLS
  value: {{ .Values.smtp.starttls | default false | quote }}
- name: SSMTP_AUTHUSER
  value: {{ .Values.smtp.username | quote }}
- name: SSMTP_AUTHPASS
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-smtp
      key: password
{{- end }}

{{- define "drupal.env" }}
- name: SILTA_CLUSTER
  value: "1"
- name: PROJECT_NAME
  value: "{{ .Values.projectName | default .Release.Namespace }}"
- name: ENVIRONMENT_NAME
  value: "{{ .Values.environmentName }}"
{{- if .Values.mariadb.enabled }}
- name: DB_USER
  value: "{{ .Values.mariadb.db.user }}"
- name: DB_NAME
  value: "{{ .Values.mariadb.db.name }}"
- name: DB_HOST
  value: {{ .Release.Name }}-mariadb
- name: DB_PASS
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-mariadb
      key: mariadb-password
{{- end }}
- name: ERROR_LEVEL
  value: {{ .Values.php.errorLevel }}
{{- if .Values.memcached.enabled }}
- name: MEMCACHED_HOST
  value: {{ .Release.Name }}-memcached
{{- end }}
{{- if .Values.elasticsearch.enabled }}
- name: ELASTICSEARCH_HOST
  value: {{ .Release.Name }}-es
{{- end }}
{{- if or .Values.mailhog.enabled .Values.smtp.enabled }}
{{ include "smtp.env" . }}
{{- end}}
{{- if .Values.varnish.enabled }}
- name: VARNISH_ADMIN_HOST
  value: {{ .Release.Name }}-varnish
- name: VARNISH_ADMIN_PORT
  value: "6082"
- name: VARNISH_CONTROL_KEY
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-varnish
      key: control_key
{{- end }}
- name: HASH_SALT
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-drupal
      key: hashsalt
- name: DRUPAL_CONFIG_PATH
  value: {{ .Values.php.drupalConfigPath }}
{{- if ne .Values.php.newrelic.license "" }}
- name: NEWRELIC_ENABLED
  value: "true"
- name: NEWRELIC_LICENSE
  value: "{{ .Values.php.newrelic.license }}"
{{- end }}
{{- range $key, $val := .Values.php.env }}
- name: {{ $key }}
  value: {{ $val | quote }}
{{- end }}
{{- range $index, $mount := $.Values.mounts }}
{{- if eq $mount.enabled true }}
- name: {{ regexReplaceAll "[^[:alnum:]]" $index "_" | upper }}_PATH
  value: {{ $mount.mountPath }}
{{- end }}
{{- end }}
{{- end }}

{{- define "drupal.tolerations" -}}
{{- range $key, $label := $ }}
- key: {{ $key }}
  operator: Equal
  value: {{ $label }}
{{- end }}
{{- end }}

{{- define "drupal.basicauth" }}
  {{- if .Values.nginx.basicauth.enabled }}
  satisfy any;
  allow 127.0.0.1;
  {{- range .Values.nginx.noauthips }}
  allow {{ . }};
  {{- end }}
  deny all;

  auth_basic "Restricted";
  auth_basic_user_file /etc/nginx/.htaccess;
  {{- end }}
{{- end }}

{{- define "drupal.wait-for-db-command" }}
TIME_WAITING=0
echo "Waiting for database.";
until mysqladmin status --connect_timeout=2 -u $DB_USER -p$DB_PASS -h $DB_HOST --silent; do
  echo -n "."
  sleep 5
  TIME_WAITING=$((TIME_WAITING+5))

  if [ $TIME_WAITING -gt 90 ]; then
    echo "Database connection timeout"
    exit 1
  fi
done
{{- end }}

{{- define "drupal.wait-for-elasticsearch-command" }}
TIME_WAITING=0
echo -n "Waiting for Elasticsearch.";
until curl --silent --connect-timeout 2 "$ELASTICSEARCH_HOST:9200" ; do
  echo -n "."
  sleep 5
  TIME_WAITING=$((TIME_WAITING+5))

  if [ $TIME_WAITING -gt 300 ]; then
    echo "Elasticsearch connection timeout"
    exit 1
  fi
done
{{- end }}

{{- define "drupal.installation-in-progress-test" -}}
-f /app/web/sites/default/files/_installing
{{- end -}}

{{- define "drupal.post-release-command" -}}
set -e

{{ if and .Release.IsInstall .Values.referenceData.enabled -}}
{{ include "drupal.import-reference-files" . }}
{{- end }}

{{ include "drupal.wait-for-db-command" . }}

{{ if .Release.IsInstall }}
touch /app/web/sites/default/files/_installing
{{- if .Values.referenceData.enabled }}
{{ include "drupal.import-reference-db" . }}
{{- end }}

{{ if .Values.elasticsearch.enabled }}
{{ include "drupal.wait-for-elasticsearch-command" . }}
{{ end }}

{{ .Values.php.postinstall.command}}
rm /app/web/sites/default/files/_installing
{{ end }}
{{ .Values.php.postupgrade.command}}

# Wait for background imports to complete.
wait

{{- if and .Values.referenceData.enabled .Values.referenceData.updateAfterDeployment }}
{{- if eq .Values.referenceData.referenceEnvironment .Values.environmentName }}
{{ include "drupal.extract-reference-data" . }}
{{- end }}
{{- end }}
{{- end }}


{{- define "drupal.extract-reference-data" -}}
set -e
if [[ "$(drush status --fields=bootstrap)" = *'Successful'* ]] ; then
  # Figure out which tables to skip.
  IGNORE_TABLES=""
  IGNORED_TABLES=""
  for TABLE in `drush sql-query "show tables;" | grep -E '{{ .Values.referenceData.ignoreTableContent }}'` ;
  do
    IGNORE_TABLES="$IGNORE_TABLES --ignore-table=$DB_NAME.$TABLE";
    IGNORED_TABLES="$IGNORED_TABLES $TABLE";
  done

  echo "Dump reference database."
  mysqldump -u $DB_USER --password=$DB_PASS --host=$DB_HOST $IGNORE_TABLES $DB_NAME > /tmp/db.sql
  mysqldump -u $DB_USER --password=$DB_PASS --host=$DB_HOST --no-data $DB_NAME $IGNORED_TABLES >> /tmp/db.sql

  # Compress the database dump and copy it into the backup folder.
  # We don't do this directly on the volume mount to avoid sending the uncompressed dump across the network.
  gzip -9 /tmp/db.sql
  cp /tmp/db.sql.gz /app/reference-data/db.sql.gz

  {{ range $index, $mount := .Values.mounts -}}
  {{- if eq $mount.enabled true -}}
  # File backup for {{ $index }} volume.
  echo "Dump reference files for {{ $index }} volume."

  # Update reference data files.
  rsync -rvu "{{ $mount.mountPath }}/" \
    --max-size="{{ $.Values.referenceData.maxFileSize }}" \
    {{ range $folderIndex, $folderPattern := $.Values.referenceData.ignoreFolders -}}
    --exclude="{{ $folderPattern }}" \
    {{ end -}}
    --delete \
    /app/reference-data/{{ $index }}
  {{ end -}}
  {{- end }}
else
  echo "Drupal is not installed, skipping reference database dump."
fi
{{- end }}

{{- define "drupal.import-reference-db" -}}
if [ -f /app/reference-data/db.sql.gz ]; then
  echo "Dropping old database"
  drush sql-drop -y

  echo "Importing reference database dump"
  cat /app/reference-data/db.sql.gz | gunzip | drush sql-cli
else
  printf "\e[33mNo reference data found, please install Drupal or import a database dump. See release information for instructions.\e[0m\n"
fi
{{- end }}

{{- define "drupal.import-reference-files" -}}
  {{ range $index, $mount := .Values.mounts -}}
  {{- if eq $mount.enabled true -}}
  if [ -d "/app/reference-data/{{ $index }}" ]; then
    echo "Importing {{ $index }} files"
    rsync -r "/app/reference-data/{{ $index }}/" "{{ $mount.mountPath }}" &
  fi
  {{ end -}}
  {{- end }}
{{- end }}