#!/usr/bin/env python

import os, sys, requests, json, yaml

def github_request(resource):

	cache_dir = '/tmp/auth_cache/' + resource

	# Create cache folder
	if not os.path.exists(os.path.dirname(cache_dir)):
		os.makedirs(os.path.dirname(cache_dir))

	# Try reading from existing key cache 
	try:
		with open(cache_dir + '.json','r') as f:
			content = f.read()
			return json.loads(content)

	# Cache not exist, redownload
	except IOError as x:

		url = "https://api.github.com/" + resource + "?per_page=100"
		res=requests.get(url,headers={"Authorization": "token " + github_token})
		response_json=res.json()
		while 'next' in res.links.keys():
			res=requests.get(res.links['next']['url'],headers={"Authorization": "token " + github_token})
			response_json.extend(res.json())
		
		# Save to cache file
		with open(cache_dir + '.json','w') as f:
			f.write(json.dumps(response_json))

		return response_json

def github_keys(login):
	
	keys_cache_dir = '/tmp/auth_cache/users/'

	# Create cache folder
	if not os.path.exists(keys_cache_dir):
		os.makedirs(keys_cache_dir)

	# Try reading from existing key cache 
	try:
		with open(keys_cache_dir + login + '.keys','r') as f:
			keys_content = f.read()
			return keys_content

	# Key does not exist, redownload
	except IOError as x:
		url = 'https://github.com/' + login + '.keys'
		response = requests.get(url)
		
		# Save to cache file
		with open(keys_cache_dir + login + '.keys','w') as f:
			f.write(response.text)

		return response.text

# AuthorizedKeysCommand does not pass environment values
#github_token = os.getenv('GIT_AUTH_TOKEN', 'NONE')
#with open('/tmp/auth.env','w') as f:
#    f.write(github_token)

with open(os.path.join(sys.path[0], 'authorized_keys.yaml'), 'r') as f:
	config = yaml.load(f)
github_token = config['github-api-access-token']

# TODO: Get org/reponame and replace wunderio/drupal-project-k8s
# TODO: What do I do with the jumphost where i don't have the projectname? We want to allow all org members?
# TODO: Configurable auth scope org vs org/reponame?
collaborators = github_request('repos/wunderio/drupal-project-k8s/collaborators')

# TODO: Invalidate collaborator cache
# TODO: Invalidate keys based on file creation time after a sensible time
keys = ''
for collaborator in collaborators:
	if 'type' in collaborator:
		if collaborator['type'] == 'User':
			keys += github_keys(collaborator['login'])

print keys
