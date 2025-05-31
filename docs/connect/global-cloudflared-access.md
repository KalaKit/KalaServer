# How to get access globally with 'cloudflared' and Cloudflare

This section assumes you already own a domain on cloudflare.

- make sure hosts doesnt have your localhost ip
	- open 'C:\Windows\System32\drivers\etc\hosts'
	- remove or comment out '127.0.0.1   yourdomain.yourextension' at the bottom

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
