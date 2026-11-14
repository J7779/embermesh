---
id: radio/wp2-wireless-mesh-network
title: Wireless mesh network
category: radio
tags: [wireless mesh network, wmn, mesh, radio]
summary: A wireless mesh network (WMN) is a communications network made up of radio nodes organized in a mesh topology.
source: https://en.wikipedia.org/w/index.php?title=Wireless_mesh_network&oldid=1384737048
source_title: Wikipedia: Wireless mesh network
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Wireless mesh network", Wikipedia, The Free Encyclopedia (revision 1384737048), https://en.wikipedia.org/w/index.php?title=Wireless_mesh_network&oldid=1384737048 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Wireless mesh network

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **wireless mesh network** (**WMN**) is a communications network made up of radio nodes organized in a mesh topology. It can also be a form of wireless ad hoc network.

A **mesh** refers to rich interconnection among devices or nodes. Wireless mesh networks often consist of mesh clients, mesh routers and gateways. Mobility of nodes is less frequent. If nodes constantly or frequently move, the mesh spends more time updating routes than delivering data. In a wireless mesh network, topology tends to be more static, so that routes
computation can converge and delivery of data to their destinations can occur. Hence, this is a low-mobility centralized form of wireless ad hoc network. Also, because it sometimes relies on static nodes to act as gateways, it is not a truly all-wireless ad hoc network.

Mesh clients are often laptops, cell phones, and other wireless devices. Mesh routers forward traffic to and from the gateways, which may or may not be connected to the Internet. The coverage area of all radio nodes working as a single network is sometimes called a mesh cloud. Access to this mesh cloud depends on the radio nodes working together to create a radio network. A mesh network is reliable and offers redundancy. When one node can no longer operate, the rest of the nodes can still communicate with each other, directly or through one or more intermediate nodes. Wireless mesh networks can self form and self heal. Wireless mesh networks work with different wireless technologies including 802.11, 802.15, 802.16, cellular technologies and need not be restricted to any one technology or protocol.

## Features

### Architecture

Wireless mesh architecture is a first step towards providing cost effective and low mobility over a specific coverage area. Wireless mesh infrastructure is, in effect, a network of routers minus the cabling between nodes. It is built of peer radio devices that do not have to be cabled to a wired port like traditional WLAN access points (AP) do. Mesh infrastructure carries data over large distances by splitting the distance into a series of short hops. Intermediate nodes not only boost the signal, but cooperatively pass data from point A to point B by making forwarding decisions based on their knowledge of the network, i.e. perform routing by first deriving the topology of the network.

Wireless mesh networks is a relatively "stable-topology" network except for the occasional failure of nodes or addition of new nodes. The path of traffic, being aggregated from a large number of end users, changes infrequently. Practically all the traffic in an infrastructure mesh network is either forwarded to or from a gateway, while in wireless ad hoc networks or client mesh networks the traffic flows between arbitrary pairs of nodes.

If rate of mobility among nodes are high, i.e., link breaks happen frequently, wireless mesh networks start to break down and have low communication performance.

### Management

This type of infrastructure can be decentralized (with no central server) or centrally managed (with a central server). Both are relatively inexpensive, and can be very reliable and resilient, as each node needs only transmit as far as the next node. Nodes act as routers to transmit data from nearby nodes to peers that are too far away to reach in a single hop, resulting in a network that can span larger distances. The topology of a mesh network must be relatively stable, i.e., not too much mobility. If one node drops out of the network, due to hardware failure or any other reason, its neighbors can quickly find another route using a routing protocol.

### Applications

Mesh networks may involve either fixed or mobile devices. The solutions are as diverse as communication needs, for example in difficult environments such as emergency situations, tunnels, oil rigs, battlefield surveillance, high-speed mobile-video applications on board public transport, real-time racing-car telemetry, or self-organizing Internet access for communities. An important possible application for wireless mesh networks is VoIP. By using a quality of service scheme, the wireless mesh may support routing local telephone calls through the mesh. Most applications in wireless mesh networks are similar to those in wireless ad hoc networks.

Some current applications:

- U.S. military forces are now using wireless mesh networking to connect their computers, mainly ruggedized laptops, in field operations.
- **Electric smart meters** now being deployed on residences, transfer their readings from one to another and eventually to the central office for billing, without the need for human meter readers or the need to connect the meters with cables.
- The laptops in the One Laptop per Child program use wireless mesh networking to enable students to exchange files and get on the Internet even though they lack wired or cell phone or other physical connections in their area.
- **Smart home devices** such as Google Wi-Fi, Google Nest Wi-Fi, and Google OnHub support **Wi-Fi mesh (i.e., Wi-Fi ad hoc) networking**. Several manufacturers of Wi-Fi routers began offering mesh routers for home use in the mid-2010s.
- Some **communications satellite constellations** operate as a mesh network, with wireless links between adjacent satellites. Calls between two satellite phones are routed through the mesh, from one satellite to another across the constellation, without having to go through an earth station. This makes for a shorter travel distance for the signal, reducing latency, and also allows for the constellation to operate with far fewer earth stations than would be required for an equal number of traditional communications satellites. The Iridium satellite constellation, consists of 66 active satellites in a polar orbit and operates as a mesh network providing global coverage.

### Operation

The principle is similar to the way packets travel around the wired Internet – data hops from one device to another until it eventually reaches its destination. Dynamic routing algorithms implemented in each device allow this to happen. To implement such dynamic routing protocols, each device needs to communicate routing information to other devices in the network. Each device then determines what to do with the data it receives – either pass it on to the next device or keep it, depending on the protocol. The routing algorithm used should attempt to always ensure that the data takes the most appropriate (fastest) route to its destination.

### Multi-radio mesh

Multi-radio mesh refers to having different radios operating at different frequencies to interconnect nodes in a mesh. This means there is a unique frequency used for each wireless hop and thus a dedicated CSMA collision domain. With more radio bands, communication throughput is likely to increase as a result of more available communication channels. This is similar to providing dual or multiple radio paths to transmit and receive data.

### Research topics

One of the more often cited papers on wireless mesh networks identified the following areas as open research problems in 2005:

_(This section is shortened.)_

## Communities and providers

- Anyfi
- AWMN (Athens [Greece] Wireless Metropolitan Network)
- CUWiN
- Freifunk (DE) / FunkFeuer (AT) / OpenWireless (CH)
- Firechat
- Firetide
- Guifi.net
- Netsukuku
- Ninux (IT)
- NYC Mesh
- Red Hook Wi-Fi

_(Later sections of the original article were left out of this offline copy for length: the rest of Features, Examples, Protocols.)_
