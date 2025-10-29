# How to get access locally with a custom domain

- open 'C:\Windows\System32\drivers\etc\hosts'
- add '127.0.0.1   yourdomain.yourextension' at the bottom
- allow port '30000' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 30000
	- allow connection
	- profile: All
	- name: yourservername
- run the server (which listens on port 30000)
- open your browser and go to 'http://yourdomain.yourextension:30000'
