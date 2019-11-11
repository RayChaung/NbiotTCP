Refer to http://backreference.org/2010/03/26/tuntap-interface-tutorial/ 

-----
dameon, a tunneling program using udp socket supporting NBIOT or Ethernet
	for NBIOT uses -c argument
	for Ethernet uses -s argument
switch_server, a proxy udp server, redirecting two sockets between NBIOT devices and Host Computers

	|--------------|        |-----------|      |---------|
	|	NBIOT Rpi  |========|   proxy   |======|  Host   |
	|--------------|        |-----------|      |---------|

-----
To compile two programs
$ gcc daemon.c -o daemon -lpthread
$ gcc switch_server.c -o switch

-----

virtual interface setting( sudo for root authority)

$ sudo openvpn --mktun --dev tun2
$ sudo ip link set tun2 up
$ sudo ip addr add 192.168.0.2/24 dev tun2		//set virtual ip address
$ sudo ip link set dev tun2 mtu 500  			//set tun interface mtu to 500 bytes
$ ifconfig 										//check all config is right

--------

Usage: 
for daemon:
	$ sudo ./daemon -i <ifacename> [-s<proxyIP>|-c <proxyIP>] [-p <port>] [-d]
	
-i <ifacename>: Name of interface to use (mandatory)
-s<proxyIP>|-c<proxyIP>: run in Host Computers (-s), or NBIOT deivces (-c) and specify proxyIP address(mandatory)
-p <port>: specify proxy's open port (mandatory)
-d: outputs debug information while running

for switch:
	$ ./switch <Port for nbiot> <Port for host>

