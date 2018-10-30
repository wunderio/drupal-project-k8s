{{- define "drupal.release_labels" }}
app: {{ .Values.app | quote }}
version: {{ .Chart.Version }}
release: {{ .Release.Name }}
{{- end }}

{{- define "drupal.domain" -}}
{{ regexReplaceAll "[^[:alnum:]]" (.Values.environmentName | default .Release.Name) "-" | lower }}.{{ .Release.Namespace }}.{{ .Values.clusterDomain }}
{{- end -}}

{{- define "drupal.php-container" }}
image: {{ .Values.php.image | quote }}
env: {{ include "drupal.env" . }}
ports:
  - containerPort: 9000
    name: drupal
volumeMounts:
  - name: drupal-public-files
    mountPath: /var/www/html/web/sites/default/files
  {{- if .Values.privateFiles.enabled }}
  - name: drupal-private-files
    mountPath: /var/www/html/private
  {{- end }}
  - name: drupal-dbdump-volume
    mountPath: /var/backups/db
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

{{- define "drupal.volumes" }}
- name: drupal-public-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-public-files
{{- if .Values.privateFiles.enabled }}
- name: drupal-private-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-private-files
{{- end }}
- name: drupal-dbdump-volume
  persistentVolumeClaim:
    claimName: {{ .Release.Namespace }}-dbdump
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
- name: IS_REFERENCE_ENVIRONMENT
  value: {{ eq .Values.referenceEnvironment .Values.environmentName }}
- name: REFERENCE_ENVIRONMENT
  value: {{ .Values.referenceEnvironment | quote }}
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