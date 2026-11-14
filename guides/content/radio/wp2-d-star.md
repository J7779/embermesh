---
id: radio/wp2-d-star
title: D-STAR
category: radio
tags: [d-star, radio]
summary: D-STAR (Digital Smart Technologies for Amateur Radio) is a digital voice and data protocol specification for amateur radio.
source: https://en.wikipedia.org/w/index.php?title=D-STAR&oldid=1383931520
source_title: Wikipedia: D-STAR
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "D-STAR", Wikipedia, The Free Encyclopedia (revision 1383931520), https://en.wikipedia.org/w/index.php?title=D-STAR&oldid=1383931520 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# D-STAR

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**D-STAR** (**Digital Smart Technologies for Amateur Radio**) is a digital voice and data protocol specification for amateur radio. The system was developed in the late 1990s by the Japan Amateur Radio League and uses minimum-shift keying in its packet-based standard. There are other digital modes that have been adapted for use by amateurs, but D-STAR was the first that was designed specifically for amateur radio.

Several advantages of using digital voice modes are that it uses less bandwidth than older analog voice modes such as amplitude modulation and frequency modulation. The quality of the data received is also better than an analog signal at the same signal strength, as long as the signal is above a minimum threshold and as long as there is no multipath propagation.

D-STAR-compatible radios are available for HF, VHF, UHF, and microwave amateur radio bands. In addition to the over-the-air protocol, D-STAR also provides specifications for network connectivity, enabling D-STAR radios to be connected to the Internet or other networks, allowing streams of voice or packet data to be routed via amateur radio.

D-STAR-compatible radios are manufactured by Icom, Kenwood, and FlexRadio Systems.

## Technical details

The system today is capable of linking repeaters together locally and through the Internet utilizing callsigns for routing of traffic. Servers are linked via TCP/IP utilizing proprietary "gateway" software, available from Icom. This allows amateur radio operators to talk to any other amateurs participating in a particular gateway "trust" environment. The current master gateway in the United States is operated by the K5TIT group in Texas, who were the first to install a D-STAR repeater system in the U.S.

D-STAR transfers both voice and data via digital encoding over the 2 m (VHF), 70 cm (UHF), and 23 cm (1.3 GHz) amateur radio bands. There is also an interlinking radio system for creating links between systems in a local area on 10 GHz, which is valuable to allow emergency communications oriented networks to continue to link in the event of internet access failure or overload.

Within the D-STAR Digital Voice protocol standards (DV), voice audio is encoded as a 3,600 bit/s data stream using proprietary AMBE encoding, with 1,200 bit/s FEC, leaving 1,200 bit/s for an additional data "path" between radios utilizing DV mode. On air bit rates for DV mode are 4,800 bit/s over the 2 m, 70 cm and 23 cm bands.

In addition to digital voice mode (DV), a Digital Data (DD) mode can be sent at 128 kbit/s only on the 23 cm band. A higher-rate data protocol, currently believed to be much like ATM, is used in the 10 GHz "link" radios for site-to-site links.

Radios providing DV data service within the low-speed voice protocol variant typically use an RS-232 or USB connection for low-speed data (1,200 bit/s), while the Icom ID-1 and IC-9700 radios offer a standard Ethernet connection for high-speed (128 kbit/s) connections on the 23 cm band. This allows easy interfacing with computer equipment.

### Gateway server

The current gateway control software rs-rp2c version G2, more commonly called "Gateway 2.0". Though most Linux distributions should be suitable, the recommended configuration uses CentOS Linux 5.1 with the latest updates, typically running (kernel 2.4.20. glibc 2.3.2 and BIND 9.2.1 or later). The CPU should be 2.4 GHz or faster and the memory should at least be 512 MB or greater. There should be two network interface cards and at least 10 GB free of hard drive space which includes the OS install. Finally for middleware, Apache 2.0.59, Tomcat 5.5.20, mod_jk2 2.0.4, OpenSSL 0.9.8d, Java SE 5.0 and postgreSQL 8.2.3 are utilized, but these can be different as updates occur.

Along with the open-source tools, the Icom proprietary dsipsvd or "D-STAR IP Service Daemon" and a variety of crontab entries utilize a mixture of the local PostgreSQL and BIND servers to look up callsigns and "pcname" fields (stored in BIND) which are mapped to individual 10.x.x.x internal-only addresses for routing of both voice and data traffic between participating gateways.

During installation, the Gateway 2.0 software installation script builds most of the Web-based open-source tools from source for standardization purposes, while utilizing some of the packages of the host Linux system, thus making CentOS 5.1 the common way to deploy a system, to keep incompatibilities from occurring in both package versions and configuration.

Additionally, gateways operating on the U.S. trust server are asked during initial setup to install DStarMonitor which is an add-on tool that allows the overall system administrators to see the status of each Gateway's local clock and other processes and PIDs needed for normal system operation, and also sends traffic and other data to servers operated under the domain name of "dstarusers.org". By this means a complete tracking of user behaviour is technically possible. Installation of this software also includes JavaAPRSd, a Java-based APRS interface which is utilized on Gateway 2.0 systems to interface between the Icom/D-STAR GPS tracking system called DPRS to the more widely known and utilized amateur radio APRS system.

#### How Gateway G2 works

Each participating amateur station wanting to use repeaters/gateways attached to a particular trust server domain must "register" with a gateway as their "home" system, which also populates their information into the trust server—a specialized central gateway system—which allows for lookups across a particular trust server domain. Only one "registration" per trust domain is required. Each amateur is set aside eight 10.x.x.x internal IP addresses for use with their callsign or radios, and various naming conventions are available to utilize these addresses if needed for specialized callsign routing. Most amateurs will need only a handful of these "registered" IP addresses, because the system maps these to callsigns, and the callsign can be entered into multiple radios.

The gateway machine controls two network interface controllers, the "external" one being on a real 10.x.x.x network behind a router. A router that can perform network address translation on a single public IP address (can be static or dynamic in Gateway G2 systems) to a full 10.x.x.x/8 network is required. From there, the Gateway has another NIC connected directly to the D-STAR repeater controller via 10BASE-T and the typical configuration is a 172.16.x.x (/24) pair of addresses between the gateway and the controller.

#### Differences between Gateway V1 and G2

The main differences between Gateway V1 and G2 are the addition of a relational database (PostgreSQL) for more flexibility and control of updates, versus the previous use of only BIND for "database" activities, the addition of both an administrative and end-user Web interface for registration which was previously handled via command-line commands by the Gateway V1 system administrators, dropping the requirement for static public IP addresses for gateways, and the ability of the software to use a fully qualified domain name to find and communicate with the trust server, allowing for redundancy/failover options for the trust server administrators. Finally, a feature called "multicast" has been added for administrators to be able to provide users with a special "name" they can route calls to which will send their transmissions to up to ten other D-STAR repeaters at the same time. With cooperation between administrators, a "multicast group" can be created for multiple repeater networks or other events.

Another additional feature of Gateway G2 is the ability to use callsign "suffixes" appended to the user's callsign in a similar fashion to the repeaters and gateways in the original system, which allow for direct routing to a particular user's radio or between two user radios with the same base callsign, by utilizing the 8th most significant field of the callsign and adding a letter to that location, both in the gateway registration process on the web interface, and in the radios themselves.

#### Gateway V1 control software

The Gateway V1 software was similar to Gateway G2, and utilized Fedora Core 2+ or Red Hat Linux 9+ OS on a Pentium-grade 2.4 GHz or faster machine.

#### IrcDDBGateway

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Technical details, Criticisms, Non-Icom D-STAR repeaters, Compatible programs and projects, Home-brew D-STAR radio, Equipment, Journal.)_
