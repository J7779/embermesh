---
id: radio/delay-tolerant-networking
title: Delay-tolerant networking
category: radio
tags: [radio, DTN, store and forward, networking]
summary: "Delay-tolerant networking: store-carry-forward delivery when no end-to-end path exists."
source: "https://en.wikipedia.org/w/index.php?title=Delay-tolerant_networking&oldid=1378840877"
source_title: Delay-tolerant networking (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Delay-tolerant networking' (rev 1378840877), Wikipedia, https://en.wikipedia.org/wiki/Delay-tolerant_networking. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Delay-tolerant networking

Delay-tolerant networking (DTN) is an approach to computer network architecture that seeks to address the technical issues in heterogeneous networks that may lack continuous network connectivity. Examples of such networks are those operating in mobile or extreme terrestrial environments, or planned networks in space.

Recently, the term disruption-tolerant networking has gained currency in the United States due to support from DARPA, which has funded many DTN projects. Disruption may occur because of the limits of wireless radio range, sparsity of mobile nodes, energy resources, attack, and noise.

## History

### Background

One of the first known references of the study of interplanetary communication comes in 1954 with the development of the Sputnik 1 satellite by the Soviet Union when Mikhail Tikhonravov emphasized that an artificial satellite is an inevitable stage in the development of rocket equipment, after which interplanetary communication would become possible. With the successful launch of Sputnik 1 in 1957, the United States entered into the Sputnik crisis and, more generally, the Space Race with the Soviet Union.

One of the direct results of the Sputnik crisis was the creation of the Advanced Research Projects Agency (ARPA), known today as Defense Advanced Research Projects Agency or DARPA. At this time, computers were making the transition from a vacuum tube-based architecture to a transistors architecture, where computers were evolving into general purpose machines. To spur research in areas relating to the feasibility of human space flight, ARPA issued a numerous government grants to both academic institution and industries to research technical details of communication between Earth and an orbiting satellite.

With the increased understanding of how to communicate with an orbiting satellite, the concept of contact points began to become a key focus in the research in space communication. The key idea behind a contact point is that there is only a set duration of time where both the source and the receiver points are able to communicate with one another. The concept of contact points was generalized further in later work to involve more complex interactions, such as planets, stars, or moons blocking the communication path between two points.

### Early research efforts

In the 1970s, spurred by the decreasing size of computers, researchers began developing technology for routing between non-fixed locations of computers.

Research began as projects under United States government grants relating to the necessity of networking technologies that can sustain the significant delays and packet corruption of space travel. Initially, these projects looked only short-range communication between crewed missions to the moon and back, but the field quickly expanded into an entire sub-field of DTNs that created the technological advances to allow for the Interplanetary Internet.

### Mobile Ad Hoc Networking (MANET) research

With the exclusion of Earth-based communication to space, there was little research in the 1980s and early 1990s in communication with the presence of a delay or disruption. With the micronization of computers, the 1990s brought about the fields of mobile ad hoc routing and vehicular ad hoc networking as areas of increased interest.

With the widespread use of 802.11 protocols for personal communications and the expanded use of mobile phones, there was a significant increase in self-organizing wireless ad hoc networking and network devices. This area of research focused on topics ranging from low-level physical transmission of signal (CDMA, CSMA, TDMA, and others) all the way to high-level protocol details (optimization of TCP for MANETs).

Concurrently with (but separate from) the MANET activities, DARPA had funded NASA, MITRE and others to develop a proposal for the Interplanetary Internet (IPN). Internet pioneer Vint Cerf and others developed the initial IPN architecture, relating to the necessity of networking technologies that can cope with the significant delays and packet corruption of deep-space communications.

### 2000

In the 2000s, many of the concepts seen in used in the early stages of the interplanetary communication and the developments made during the 1990s in MANETs were combined and used to develop and deploy a complete networking on satellites orbiting numerous planets. This network, often referred to as the Interplanetary Internet, has helped to unify the research of MANETs and traditional interplanetary communication into a common area of delay-tolerant networking.

In 2002, Kevin Fall started to adapt some of the ideas in the IPN design to terrestrial networks and coined the term *delay-tolerant networking* and the DTN acronym. A paper published in 2003 SIGCOMM conference gives the motivation for DTNs.

The mid-2000s brought about increased interest in DTNs, including a growing number of academic conferences on delay and disruption-tolerant networking, and growing interest in combining work from sensor networks and MANETs with the work on DTN, as well as the increasing complexity of the Interplanetary Internet. This field saw many optimizations on classic ad hoc and delay-tolerant networking algorithms and began to examine factors such as security, reliability, verifiability, and other areas of research that are well understood in traditional computer networking.

## Routing

The ability to transport, or route, data from a source to a destination is a fundamental ability all communication networks must have. Delay and disruption-tolerant networks (DTNs), are characterized by their lack of connectivity, resulting in a lack of instantaneous end-to-end paths. In these challenging environments, popular ad hoc routing protocols such as AODV and DSR fail to establish routes. This is due to these protocols trying to first establish a complete route and then, after the route has been established, forward the actual data. However, when instantaneous end-to-end paths are difficult or impossible to establish, routing protocols must take to a "store and forward" approach, where data is incrementally moved and stored throughout the network in hopes that it will eventually reach its destination. A common technique used to maximize the probability of a message being successfully transferred is to replicate many copies of the message in the hope that one will succeed in reaching its destination. This is feasible only on networks with large amounts of local storage and internode bandwidth relative to the expected traffic. In many common problem spaces, this inefficiency is outweighed by the increased efficiency and shortened delivery times made possible by taking maximum advantage of available unscheduled forwarding opportunities. In others, where available storage and internode throughput opportunities are more tightly constrained, a more discriminate algorithm is required.

## Other concerns

### Bundle protocols

In efforts to provide a shared framework for algorithm and application development in DTNs, RFC 4838 and 5050 were published in 2007 to define a common abstraction to software running on disrupted networks. Commonly known as the Bundle Protocol, this protocol defines a series of contiguous data blocks as a bundle—where each bundle contains enough semantic information to allow the application to make progress where an individual block may not. Bundles are routed in a store and forward manner between participating nodes over varied network transport technologies (including both IP and non-IP based transports). The transport layers carrying the bundles across their local networks are called *bundle convergence layers.* The bundle architecture therefore operates as an overlay network, providing a new naming architecture based on Endpoint Identifiers (EIDs) and coarse-grained class of service offerings.

Protocols using bundling must leverage application-level preferences for sending bundles across a network. Due to the store and forward nature of delay-tolerant protocols, routing solutions for delay-tolerant networks can benefit from exposure to application-layer information. For example, network scheduling can be influenced if application data must be received in its entirety, quickly, or without variation in packet delay. Bundle protocols collect application data into bundles that can be sent across heterogeneous network configurations with high-level service guarantees. The service guarantees are generally set by the application level, and the RFC 5050 Bundle Protocol specification includes "bulk", "normal", and "expedited" markings.

In October 2014 the Internet Engineering Task Force (IETF) instantiated a Delay Tolerant Networking working group to review and revise the protocol specified in RFC 5050. The Bundle Protocol for CCSDS is a profile of RFC 5050 specifically addressing the Bundle Protocol's utility for data communication in space missions.

As of January 2022, the IETF published the following RFCs related to BPv7: RFC 9171, 9172, 9173, 9174. In January 2025, RFC 9713 was published, which updates RFC 9171.

### Security issues

Addressing security issues has been a major focus of the bundle protocol. Possible attacks take the form of nodes behaving as a "black hole" or a "flooder".

Security concerns for delay-tolerant networks vary depending on the environment and application, though authentication and privacy are often critical. These security guarantees are difficult to establish in a network without continuous bi-directional end-to-end paths between devices because the network hinders complicated cryptographic protocols, hinders key exchange, and each device must identify other intermittently visible devices. Solutions have typically been modified from mobile ad hoc network and distributed security research, such as the use of distributed certificate authorities and PKI schemes. Original solutions from the delay-tolerant research community include: 1) the use of identity-based encryption, which allows nodes to receive information encrypted with their public identifier; and 2) the use of tamper-evident tables with a gossiping protocol;

## Implementations

There are a number of implementations of the Bundle Protocol:

### BPv6 (RFC 5050, Bundle Protocol for CCSDS)

The main implementation of BPv6 are listed below. A number of other implementations exist.

- High-rate DTN—C++17 - based; performance-optimized DTN; runs directly on Linux and Windows.
- Interplanetary Overlay Network (ION)—Written in C; designed to run on a wide variety of platforms; conforms to restrictions for space flight software (e.g. no dynamic memory allocation).
- IBR-DTN—C++ - based; runs on routers with OpenWRT; also contains Java applications (router and user apps) for use on Android.
- DTN2—C++ - based; designed to be a reference / learning / teaching implementation of the Bundle Protocol.
- DTN Marshal Enterprise (DTNME)—C++ - based; Enterprise solution; designed as an operational DTN implementation. Currently used in ISS operations. DTNME is a single implementation supporting both BPv6 and BPv7.

### BPv7

The following BPv7 implementations exist among others and are partially listed in the BPv7 RFC draft.

- High-rate DTN—C++17 - based; performance-optimized DTN; runs directly on Linux and Windows.
- μPCN—C; built upon the POSIX API as well as FreeRTOS and intended to run on low-cost micro satellites.
- μD3TN—C; actively maintained, space-tested DTN protocol implementation initially based on μPCN.
- PyDTN—Python; developed by X-works and during the IETF 101 Hackathon.
- Terra—Java; developed in the context of terrestrial DTN.
- dtn7-go—Go; implementation focused on easy extensibility and suitable for research.
- dtn7-rs—Rust; intended for environments with limited resources and performance requirements.
- Interplanetary Overlay Network (ION)—C; intended to be usable in embedded environments including spacecraft flight computers.
- DTN Marshal Enterprise (DTNME)—C++ - based; Enterprise solution; designed as an operational DTN implementation. Currently used in ISS operations. DTNME is a single implementation supporting both BPv6 and BPv7.
- NASA BPLib-C; A Bundle Protocol library and associated applications by Goddard Space Flight Center. Intended for general use, particularly in space flight applications, integration with cFS (core Flight System), and other applications where store-and-forward capabilities are needed. First time will be used on PACE mission

## Research efforts

Various research efforts are currently investigating the issues involved with DTN:

- The Delay-Tolerant Networking Research Group.
- The Technology and Infrastructure for Developing Regions project at UC Berkeley
- The Bytewalla research project at the Royal Institute of Technology, KTH
- The KioskNet research project at the University of Waterloo.
- The DieselNet Archived 2022-03-17 at the Wayback Machine research project at the University of Massachusetts Amherst, Amherst.
- The ResiliNets Research Initiative at the University of Kansas and Lancaster University.
- The Haggle Archived 2019-09-08 at the Wayback Machine EU research project.
- The Space Internetworking Center EU/FP7 project at the Democritus University of Thrace.
- The N4C EU/FP7 research project.
- The WNaN DARPA project.
- The EMMA and OPTRACOM projects at TU Braunschweig
- The DTN at Helsinki University of Technology.
- The SARAH Archived 2018-12-25 at the Wayback Machine project, funded by the French National Research Agency (ANR).
- The development of the DoDWAN platform at Université Bretagne Sud.
- The CROWD Archived 2011-07-21 at the Wayback Machine project, funded by the French National Research Agency (ANR).
- The PodNet project at KTH Stockholm and ETH Zurich.

Some research efforts look at DTN for the Interplanetary Internet by examining use of the Bundle Protocol in space:

- The Saratoga Archived 2009-12-12 at the Wayback Machine project at the University of Surrey, which was the first to test the bundle protocol in space on the UK-DMC Disaster Monitoring Constellation satellite in 2008.
- NASA JPL's Deep Impact Networking (DINET) Experiment on board the *Deep Impact*/EPOXI spacecraft.
- BioServe Space Technologies, one of the first payload developers to adopt the DTN technology, has utilized their CGBA (Commercial Generic Bioprocessing Apparatus) payloads on board the ISS, which provide computational/communications platforms, to implement the DTN protocol.
- NASA, ESA Use Experimental Interplanetary Internet to Test Robot From International Space Station
