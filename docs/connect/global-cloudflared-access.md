# How to get access globally with 'cloudflared' and Cloudflare

This section assumes you already own a domain on cloudflare.

- add new DNS record to cloudflare domain:
	- type: CNAME
	- name: www
	- content: yourtunnelname.yourdomain.yourextension (KalaServer.thekalakit.com)
	- proxied: true
	
- add new DNS record to cloudflare domain:
	- type: CNAME
	- name: yourdomain.yourextension
	- content: yourtunnelname.yourdomain.yourextension (KalaServer.thekalakit.com)
	- proxied: true
	
- copy 'config.yml' template from 'example' folder to your 'Users' path inside a new '.cloudflared' folder and fill the slots that contain template values

- create a cloudflared tunnel if you havent yet
	- log in to 'https://one.dash.cloudflare.com/'
	- go to networks > tunnels
	- select 'Create a tunnel'
	- choose connector type 'Cloudflared' and click next
	- enter tunnel name (needs to be save as you add to server)
	- select 'Save tunnel'
	- copy the command text in 'run the following command'
	- create a new file called tunneltoken.txt inside the folder where your server exe will go to and copy the contents of the copied text except 'cloudflared.exe service install ' inside it, make sure it has no leading or trailing spaces, just the token itself
	- WARNING: Do NOT share tunneltoken.txt, anyone with this file can hijack your server. Keep it secure like a root password.
	
- Add a new cloudflared application
	- log in to 'https://one.dash.cloudflare.com/'
	- go to access > applications
	- select 'Create an Application'
	- select 'Self-hosted'
	- Add app data
		- Application Name: yourappname
		- Domain: yourdomain.com
		- Path: /
	- Session duration: up to you
	- Policies:
		- Add policy 'Public Access'
		- Set include as 'Everyone'
	
- download [the latest cloudflared-windows-amd64.exe](https://github.com/cloudflare/cloudflared/releases/latest)
- copy to location where your server exe is and rename it to 'cloudflared.exe'
- run the server as admin with any port
- open your browser and go to 'http://yourdomain.yourextension'
