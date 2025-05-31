# How to get access locally

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