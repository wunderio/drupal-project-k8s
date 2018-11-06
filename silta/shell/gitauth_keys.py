#!/usr/bin/env python

import os, sys, requests, json, yaml, re, time

def gitauth_api_request(resource):

	cache_dir = '/tmp/gitauth_cache/' + gitauth_host + '/' + resource + '/'

	# Create cache folder
	if not os.path.exists(cache_dir):
		os.makedirs(cache_dir)

    # Removing collaborator cache when it's 1 hour old
	remove_stale_cache(cache_dir, 1)

	# Try reading from existing key cache
	try:
		with open(cache_dir + 'response.json','r') as f:
			content = f.read()
			return json.loads(content)

	# Cache not exist, redownload
	except IOError as x:

        # TODO: We can compare host and check other API providers too.
        # TODO: https://developer.atlassian.com/bitbucket/api/2/reference/resource/teams/%7Busername%7D/members
		url = "https://api.github.com/" + resource + "?per_page=100"
		res=requests.get(url,headers={"Authorization": "token " + gitauth_api_token})
		response_json=res.json()
		while 'next' in res.links.keys():
			res=requests.get(res.links['next']['url'],headers={"Authorization": "token " + gitauth_api_token})
			response_json.extend(res.json())
		
		# Save to cache file
		with open(cache_dir + 'response.json','w') as f:
			f.write(json.dumps(response_json))

		return response_json

def git_api_keys(login):
	
	keys_cache_dir = '/tmp/gitauth_cache/'+ gitauth_host + '/users/'

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
	    # TODO: url = 'https://api.bitbucket.org/2.0/users/' + login + '/ssh-keys'
		url = 'https://github.com/' + login + '.keys'
		response = requests.get(url)
		
		# Save to cache file
		with open(keys_cache_dir + login + '.keys','w') as f:
			f.write(response.text)

		return response.text

# Remove files in specified folder that are older than xx minutes
def remove_stale_cache(path, hours):

    if os.path.exists(path):
        # List files in directory
        for f in os.listdir(path):
            creation_time = os.path.getctime(path + f)
            # If the file creation time exceeds defined, remove it
            if (time.time() - creation_time) >= 3600 * hours:
                os.unlink(path + f)

#def gitauth_org(repo_url):
#    parse = re.compile(r'(?P<host>(git@|https://)([\w\.@]+)(/|:))(?P<owner>[\w,\-,\_]+)/(?P<repo>[\w,\-,\_]+)(.git){0,1}((/){0,1})')
#    match = parse.match(repo_url)
#    if match and 'owner' in match.groupdict():
#        return match.group('owner')
#    else:
#        return None
#
#def gitauth_project(repo_url):
#    parse = re.compile(r'(?P<host>(git@|https://)([\w\.@]+)(/|:))(?P<owner>[\w,\-,\_]+)/(?P<repo>[\w,\-,\_]+)(.git){0,1}((/){0,1})')
#    match = parse.match(repo_url)
#    if match and 'repo' in match.groupdict():
#        return match.group('repo')
#    else:
#        return None

def conf_read():
    global gitauth_api_token, gitauth_host, gitauth_organisation, gitauth_project

    # Read config
    with open(os.path.join(sys.path[0], 'gitauth_keys.yaml'), 'r') as f:
    	config = yaml.load(f)

    gitauth_api_token = conf_parse(config, 'gitauth-api-token')
    gitauth_host = conf_parse(config, 'gitauth-host')
    gitauth_organisation = conf_parse(config, 'gitauth-organisation')
    gitauth_project = conf_parse(config, 'gitauth-project')

def conf_parse(config, attribute):
    if config and attribute in config:
        return config[attribute]
    else:
        return None

conf_read()
keys = '';

if gitauth_organisation:

    if not gitauth_host or not gitauth_api_token:
        exit (1)

    # Removing user keys cache when it's 24 hours old
    remove_stale_cache('/tmp/gitauth_cache/'+ gitauth_host + '/users/', 24)

    if not gitauth_project:
        # No repo provided, allow all organisation members

        collaborator = gitauth_api_request('repos/' + gitauth_organisation + '/members')
        # Get the keys for each member
        for collaborator in collaborators:
            if 'type' in collaborator and collaborator['type'] == 'User':
                keys += git_api_keys(collaborator['login'])

        # Outside collaborators
        collaborator = gitauth_api_request('repos/' + gitauth_organisation + '/outside_collaborators')
        # Get the keys for each member
        for collaborator in collaborators:
            if 'type' in collaborator and collaborator['type'] == 'User':
                keys += git_api_keys(collaborator['login'])

    else:
        # We have both org and repo defined, we can get contributor list off it.
        collaborators = gitauth_api_request('repos/' + gitauth_organisation + '/' + gitauth_project + '/collaborators')

        # Get the keys for each collaborator
        for collaborator in collaborators:
            if 'type' in collaborator and collaborator['type'] == 'User':
                keys += git_api_keys(collaborator['login'])

print keys