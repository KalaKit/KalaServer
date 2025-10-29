# How to get access locally

- make sure hosts doesnt have your localhost ip
	- open 'C:\Windows\System32\drivers\etc\hosts'
	- remove or comment out '127.0.0.1   yourdomain.yourextension' at the bottom

- allow port '30000' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 30000
	- allow connection
	- profile: private
	- name: yourservername
- run the server (which listens on port 30000)
- open your browser and go to 'http://localhost:30000'