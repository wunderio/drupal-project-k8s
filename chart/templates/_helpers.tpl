{{- define "drupal.release_labels" }}
app: {{ printf "%s-%s" .Release.Name .Chart.Name | trunc 63 }}
version: {{ .Chart.Version }}
release: {{ .Release.Name }}
{{- end }}

{{- define "drupal.domain" -}}
{{ regexReplaceAll "[^[:alnum:]]" .Values.branchname "-" | lower }}.{{ .Release.Namespace }}.{{ .Values.clusterDomain }}
{{- end -}}

{{- define "drupal.php-container" }}
image: {{ .Values.drupal.image | quote }}
env: {{ include "drupal.env" . }}
ports:
  - containerPort: 9000
    name: drupal
volumeMounts:
  - name: drupal-public-files
    mountPath: /var/www/html/web/sites/default/files
  {{- if .Values.drupal.privateFiles.enabled }}
  - name: drupal-private-files
    mountPath: /var/www/html/private
  {{- end }}
{{- end }}

{{- define "drupal.volumes" }}
- name: drupal-public-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-public-files
{{- if .Values.drupal.privateFiles.enabled }}
- name: drupal-private-files
  persistentVolumeClaim:
    claimName: {{ .Release.Name }}-private-files
{{- end }}
{{- end }}

{{- define "drupal.env" }}
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
- name: HASH_SALT
  valueFrom:
    secretKeyRef:
      name: {{ .Release.Name }}-secrets-drupal
      key: hashsalt
{{- range $key, $val := .Values.drupal.env }}
- name: {{ $key }}
  value: {{ $val | quote }}
{{- end }}
{{- if .Values.drupal.privateFiles.enabled }}
- name: PRIVATE_FILES_PATH
  value: '/var/www/html/private'
{{- end }}
{{- end }}