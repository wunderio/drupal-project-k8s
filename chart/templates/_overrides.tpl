{{/*
Override templates from subcharts.
*/}}

{{- define "uname" -}}
{{ .Release.Name }}-es
{{- end }}