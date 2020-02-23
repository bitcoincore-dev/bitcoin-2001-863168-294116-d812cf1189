P2P and network changes
-----------------------

- Added NAT-PMP port mapping support via [`libnatpmp`](https://miniupnp.tuxfamily.org/libnatpmp.html)

Command-line options
--------------------

-  The `-natpmp` option has been added to use NAT-PMP to map the listening port. If both `-upnp` and `-natpmp` options are provided, the former prevails.
