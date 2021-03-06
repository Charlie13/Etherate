Etherate
========

[![Build Status](https://travis-ci.org/jwbensley/Etherate.svg?branch=master)](https://travis-ci.org/jwbensley/Etherate)
[![Bitdeli Badge](https://null.53bits.co.uk/uploads/programming/c/etherate/etherate-github-badge.png)](https://bitdeli.com/free "Bitdeli Badge")
[![PayPal Donate](https://img.shields.io/badge/paypal-donate-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=james%40bensley%2eme&lc=GB&item_name=Etherate&currency_code=GBP)


#### What is it

    Etherate is a Linux CLI program for testing layer 2 Ethernet & MPLS.


#### Why was it made

    Programs such as iPerf/jPerf/Ostinato/PathEth/Scapy (to name just a few) 
    are excellent! They can saturate a link to measure throughput or simulate
    congestion and they allow the user to set custom DSCP values to test QoS.
    They usually operate at layer 3 or 4 of the OSI model using either TCP or
    UDP for data transport. Some of them use sockets defined by the OS that
    rely on the convoluted OS TCP stack, others use 3rd party libraries (such
    as libpcap, NetMap etc).

    These programs are great for testing over a layer 3 boundary such as across
    the Internet, home broadband, client to server diagnostics etc. Etherate uses
    raw sockets within the OS operating directly over layer 2 to provide low level
    network analysis for Ethernet and MPLS connections without using any 3rd
    party libraries or kernel bypass ring buffers.

    The main goal is a free Ethernet and MPLS testing program that allows for
    advanced network analysis. With any modern day CPU and off the self NIC
    it should saturate up to a 10GigE link and allow for the testing of various
    metro and carrier Ethernet features (such tag swapping and 802.1p QoS etc).


#### Current features

    Having moved from alpha to beta the current focus is to increase the
    feature set of Etherate (also to improve efficiency and fix bugs).

    Currently working features which all operate directly over layer 2:
  
  - Any EtherType
  - Any Source MAC and/or Destination MAC
  - Any VLAN ID
  - Any priority (PCP) value
  - Supports double tagging (any inner and outer VLAN ID, any PCP)
  - Toggle DEI bit
  - Toggle frame acknowledgement
  - Optional speed limit in Mbps, MBps or Frames/p/s
  - Optional frame payload size
  - Optional transfer limit in MBs/GBs
  - Optional test duration
  - L2 storm (BUM) testing
  - One way delay measurement
  - Perform MTU sweeping test
  - Perform link quality tests (RTT & jitter)
  - Simulate MPLS label stacks
  - Insert MPLS pseudowire control word
  - Load a custom frame from file for transmission


#### Development plan

    These are features currently being worked upon:
  
  - Add BPDU & keepalive generation shortcuts
  - Report throughput if additional headers (IPv4/6/TCP/UDP) were present
  - The Etherate code is not object orientated so once the above feature list
    is completed development will cease, a new multi-threaded network testing
    app is being design from the ground up.


#### Technical details

    Etherate is a single threaded application, despite this 10G throughput
    can be achieved on a 2.4Ghz Intel CPU with 10G Intel NIC. Etherate is
    not currently using NetMap/DPDK or the similar frameworks as it is still
    in beta. This may become a future development (if the desired performance
    levels can't be reached) however this would introduce an additional
    dependency which is undesired for this project.


#### More info, usage examples, FAQs etc

    http://null.53bits.co.uk/index.php?page=etherate


#### Who made it (where to send your hate mail)

    James Bensley [jwbensley at gmail dot com]

    Major credit is due for their code review and bug fixing efforts, to;
    Bradley Kite
    Bruno de Montis
