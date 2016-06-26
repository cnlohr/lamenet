# lamenet

A way of making a really awful network connection.

Idealy, this is used on a router, or other device with two network cards.  You can execute the program to link both network cards as though it was a network link.  It can manipulate the quality.  You can use this tool to see how badly websites load over really bad connections, test online games and applications with packet loss and other adverse network conditions, like mobile, bad RF links, really really slow links and the like!

This comes pre-compiled for the OpenWRT MIPS.  This is convenient since most routers already have two ports.  It will work on just about anything that runs Linux.

Additionally, it supports full packet logging across the interface for use with any tools that can interpret pcap files.

Usage:

```
 ./linkpair [interface1] [interface2] [additional options]
  Options:
   -p [percent pass]
   -d [per-packet blocking time in ms]
   -a [all stop time to happen every second (in ms)]
   -b [base, per-packet delay in ms]
   -t [additional random delay time per packet in ms]
   -l [pcap file for logging]
```

For instance, a 100% up link, with predictable high latency would be called using:

```
  ./linkpair eth0 eth1 -p 100 -d 1 -a 1 -b 1000 -t 1
```

Default values for a pretty lame link are already set, so you need not specify parameters.
