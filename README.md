# New Version to be released soon

A new version of basebox(d) will be released on July 1st, 2016 including
support for Broadcom's OFDPA 2.0.

If you are looking for older versions of basebox(d) for controlling 
ovs and xdpd, you find those in the swx branch on github [2].

[1] https://github.com/Broadcom-Switch/of-dpa/

[2] https://github.com/bisdn/basebox/tree/swx


# What it is

baseboxd is a tiny OpenFlow controller for mapping legacy protocol
stacks on an OpenFlow datapath element. It is merely a fun project
and a playground for testing architectures and strategies for 
emulating legacy protocol stacks on OpenFlow.

In its most basic form it turns an OpenFlow pipeline into a basic
routing switch with Layer 2 and Layer 3 forwarding capabilities.
baseboxd aims towards easy extensibility for adding new protocols
like GTP, GRE, and so on that typically require basic Ethernet and 
or IP support.

# Version

There is no stable release yet. Current development version is
v0.6.0dev and v0.6 is supposed to be the first useful release
of baseboxd.

# Requirements

- A modern GNU build-system (autoconf, automake, libtool, ...)
- GNU/Linux and libc development headers
- pkg-config
- ROFL libraries installed, see [1] and [2]
- Configuration File Library (libconfig) [3]
- Network Protocol Library Suite 3.2.x (libnl) [4]
- [optional] if you want to run automatic tests (make check), libcunit and libcppunit are required.

[1] http://www.roflibs.org/ 

[2] https://github.com/bisdn/rofl-core 

[3] http://www.hyperrealm.com/libconfig/ 

[4] http://www.infradead.org/~tgr/libnl/ 

# Installation

Install the dependencies and run:

        sh# ./autogen.sh
        sh# cd build
        sh# ../configure
        sh# make
        sh# make install

# Configuration

Currently, baseboxd uses a configuration file for storing control 
and management information. An example baseboxd.conf file is available
in directory `basebox/src/baseboxd`. See below:

```
#
# baseboxd.conf
#

baseboxd: {

	# common daemon parameters
	#
	daemon: {
		# daemonize
		#
		daemonize = false;

		# pid-file
		#
		pidfile = "/var/run/baseboxd.pid";
		
		# log-file
		#
		logfile = "/var/log/baseboxd.log";
		
		# debug level
		#
		logging: {
			rofl: {
				debug = 0;
			};
			core: {
				debug = 0;
			};
		};
	};

	# usecase specific configuration
	#
	usecase: {
		# business logic to be loaded into baseboxd
		#
		script = "/usr/local/sbin/baseboxd.py";
	};

	# OpenFlow endpoint configuration
	#
	openflow: {
	
		# set OpenFlow version
		#
		version = 4;

		# binding address
		#
		bindaddr = "::";
		
		# listening port
		#
		bindport = 6653;
	};

	# configuration for roflibs libraries
	# 
	roflibs:{

		# ethcore related configuration
		#
		# This section contains configuration parameters for ethcore
		# and defines virtual LANs emulated on the datapath element.
		# baseboxd is capable of handling an arbitrary number of datapath
		# elements in principle, only limited by available processing 
		# and memory resources.
		ethcore:{

			# This is a list of group entries for datapath specific 
			# configuration options. 
			#
			datapaths = ( {
			
				# [dpid] (mandatory)
				#
				# defines the OpenFlow datapath identifier as decimal number
				#
				dpid = 256;
	
				# [default_pvid] (optional)
				#
				# The default VLAN identifier assigned to untagged frames
				# received on a port without any explicit definition (see below)
				# This creates a new logical VLAN on the datapath element with
				# the specified VID.
				#
				default_pvid = 1;	
				
				# [ports] (optional)
				#
				# A list of group entries defining port memberships for individual
				# ports. Each entry refers to a physical or logical port attached
				# to the OpenFlow datapath. A port entry contains the following 
				# entries:
				#
				#   [portno] (mandatory)
				#   The OpenFlow port number assigned by the datapath element to 
				#   a specific port.
				#
				#   [pvid] (optional)
				#   The port VID assigned to this port. This VID is assigned to all
				#   untagged frames received on the specific port.
				#
				#   [vlans] (optional)
				#   A list of tagged port memberships for the specified port. Frames
				#   received with a VID not specified in list vlans will be silently dropped.
				#
				ports = ( { portno=1; pvid=16; vlans=(10, 20, 128); },
				          { portno=2; pvid=32; vlans=(10, 128); },
				          { portno=3; pvid=48; vlans=(128); },
					  { portno=4; pvid=64; } );
	
				# [ethernet] (optional)
				#
				# Predefined endpoints for layer (n+1) Ethernet communication.
				# A list of group entries defining Ethernet endpoints for higher layer
				# communication (e.g. IPv4/v6). Each Ethernet endpoint is attached to 
				# a certain VID:mac-address combination. baseboxd creates tap-devices
				# for each endpoint defined, so make sure to choose unique devnames.
				#
				#   [devname] (mandatory)
				#   Name assigned to the new Ethernet endpoint and port created in Linux.
				#
				#   [vid] (optional)
				#   The VLAN identifier assigned to this endpoint, i.e. packets sent
				#   via this interface will enter the specified VLAN. If no vid parameter
				#   is specified, the default port VID defined for this datapath element
				#   will be used.
				#
				#   [hwaddr] (mandatory)
				#   MAC address assigned to this Ethernet endpoint. Arbitrary values 
				#   may be chosen for the link layer address, but should fulfill
				#   constraints and regulations defined by IANA/IEEE/EUI48 ;)
				#
				ethernet = ( { devname="ep1"; vid=16; hwaddr="00:11:11:11:11:11"; },
				   	     { devname="ep2"; vid=32; hwaddr="00:22:22:22:22:22"; },
					     { devname="ep3"; vid=48; hwaddr="00:33:33:33:33:33"; } );
			} );
		};

		# ipcore related configuration
		#
		# There are no ipcore specific configuration parameters defined 
		# currently. However, when creating Ethernet endpoints you may use 
		# your Linux distribution specific tools for assigning IP addresses
		# and routes. baseboxd will listen to the netlink interface and map
		# addresses and routes to appropriate OpenFlow flow table entries
		# on the OpenFlow datapath element.
		#
		# User plane traffic will be directly switched in the datapath 
		# element, while control plane traffic (e.g., ARP/ICMP, etc.) will 
		# be escalated to the Linux kernel for further handling.
		#
		ipcore:{

			# nothing yet
		};
	};
};
```

baseboxd is capable of handling multiple datapath elements in
parallel.  Upon establishment of an OpenFlow control connection,
baseboxd creates virtual Ethernet endpoints mapped to virtual ports
using device /dev/net/tun.  Please make sure to start baseboxd
with suitable administrative rights.  You may configure these new
virtual ports according to your needs, e.g., assigning IP addresses
and routes. baseboxd will map this information down to the attached
datapath element and establish shortcuts for packet forwarding.

