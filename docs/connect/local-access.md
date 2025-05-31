# How to get access locally

- allow port '8080' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 8080
	- allow connection
	- profile: private
	- name: yourservername
- run the server (which listens on port 8080)
- open your browser and go to 'http://localhost:8080'