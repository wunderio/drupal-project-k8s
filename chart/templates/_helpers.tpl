{{- define "drupal.release_labels" -}}
app: {{ .Values.app | quote }}
release: {{ .Release.Name }}
{{- end }}

{{- define "drupal.domain" -}}
{{ include "drupal.environmentName" . }}.{{ .Release.Namespace }}.{{ .Values.clusterDomain }}
{{- end -}}

{{- define "drupal.environmentName" -}}
{{ regexReplaceAll "[^[:alnum:]]" (.Values.environmentName | default .Release.Name) "-" | lower | trunc 50 | trimSuffix "-" }}
{{- end -}}

{{- define "drupal.referenceEnvironment" -}}
{{ regexReplaceAll "[^[:alnum:]]" .Values.referenceData.referenceEnvironment "-" | lower | trunc 50 | trimSuffix "-" }}
{{- end -}}

{{- define "drupal.environment.hostname" -}}
{{ regexReplaceAll "[^[:alnum:]]" (.Values.environmentName | default .Release.Name) "-" | lower | trunc 50 | trimSuffix "-" }}
{{- end -}}

{{- define "drupal.php-container" -}}
image: {{ .Values.php.image | quote }}
env: {{ include "drupal.env" . }}
ports:
  - containerPort: 9000
    name: drupal
{{- end }}

{{- define "drupal.volumeMounts" -}}
- name: drupal-public-files
  mountPath: /var/www/html/web/sites/default/files
{{- if .Values.privateFiles.enabled }}
- name: drupal-private-files
  mountPath: /var/www/html/private
{{- end }}
- name: php-conf
  mountPath: /etc/php7/php.ini
  readOnly: true
  subPath: php_ini
- name: php-conf
  mountPath: /etc/php7/php-fpm.conf
  readOnly: true
  subPath: php-fpm_conf
- name: php-conf
  mountPath: /etc/php7/php-fpm.d/www.conf
  readOnly: true
  subPath: www_conf
{{- end }}

{{- define "drupal.volumes" -}}
- name: drupal-public-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-public-files
{{- if .Values.privateFiles.enabled }}
- name: drupal-private-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-private-files
{{- end }}
- name: php-conf
  configMap:
    name: {{ .Release.Name }}-php-conf
    items:
      - key: php_ini
        path: php_ini
      - key: php-fpm_conf
        path: php-fpm_conf
      - key: www_conf
        path: www_conf
{{- end }}

{{- define "drupal.imagePullSecrets" }}
{{- if .Values.imagePullSecrets }}
imagePullSecrets:
{{ .Values.imagePullSecrets | toYaml }}
{{- end }}
{{- end }}

{{- define "drupal.env" }}
- name: SILTA_CLUSTER
  value: "1"
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
{{- if .Values.memcached.enabled }}
- name: MEMCACHED_HOST
  value: {{ .Release.Name }}-memcached
{{- end }}
{{- if .Values.elasticsearch.enabled }}
- name: ELASTIC_HOST
  value: {{ .Release.Name }}-elastic
{{- end }}
- name: HASH_SALT
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-drupal
      key: hashsalt
{{- range $key, $val := .Values.php.env }}
- name: {{ $key }}
  value: {{ $val | quote }}
{{- end }}
{{- if .Values.privateFiles.enabled }}
- name: PRIVATE_FILES_PATH
  value: '/var/www/html/private'
{{- end }}
{{- end }}

{{- define "drupal.basicauth" }}
  {{- if .Values.nginx.basicauth.enabled }}
  satisfy any;
  allow 127.0.0.1;
  {{- range .Values.nginx.basicauth.noauthips }}
  allow {{ . }};
  {{- end }}
  deny all;

  auth_basic "Restricted";
  auth_basic_user_file /etc/nginx/.htaccess;
  {{- end }}
{{- end }}

{{- define "drupal.wait-for-db-command" }}
TIME_WAITING=0
  until mysqladmin status --connect_timeout=2 -u $DB_USER -p$DB_PASS -h $DB_HOST --silent; do
  echo "Waiting for database..."; sleep 5
  TIME_WAITING=$((TIME_WAITING+5))

  if [ $TIME_WAITING -gt 90 ]; then
    echo "Database connection timeout"
    exit 1
  fi
done
{{- end }}

{{- define "drupal.wait-for-ref-fs-command" }}
TIME_WAITING=0
until touch /var/www/html/reference-data/_fs-test; do
  echo "Waiting for reference-data fs..."; sleep 2
  TIME_WAITING=$((TIME_WAITING+2))

  if [ $TIME_WAITING -gt 20 ]; then
    echo "Reference data filesystem timeout"
    exit 1
  fi
done
rm /var/www/html/reference-data/_fs-test
{{- end }}

{{- define "drupal.deployment-in-progress-test" -}}
-f /var/www/html/web/sites/default/files/_deployment
{{- end -}}

{{- define "drupal.post-release-command" -}}
set -e

{{- if eq .Values.referenceData.referenceEnvironment .Values.environmentName }}
{{ include "drupal.wait-for-ref-fs-command" . }}
{{- end }}

{{ include "drupal.wait-for-db-command" . }}

{{ if .Release.IsInstall }}
touch /var/www/html/web/sites/default/files/_deployment
{{ .Values.php.postinstall.command}}
rm /var/www/html/web/sites/default/files/_deployment
{{ else }}
{{ .Values.php.postupgrade.command}}
{{ end }}

{{- if and .Values.referenceData.enabled .Values.referenceData.updateAfterDeployment }}
{{- if eq .Values.referenceData.referenceEnvironment .Values.environmentName }}
{{ .Values.referenceData.command }}
{{- end }}
{{- end }}
{{- end }}
