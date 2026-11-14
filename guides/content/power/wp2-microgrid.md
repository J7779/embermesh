---
id: power/wp2-microgrid
title: Microgrid
category: power
tags: [microgrid, stand-alone, isolated microgrid, islandable microgrid, nanogrid, energy]
summary: A microgrid is a local electrical grid with defined electrical boundaries, acting as a single and controllable entity.
source: https://en.wikipedia.org/w/index.php?title=Microgrid&oldid=1384014039
source_title: Wikipedia: Microgrid
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Microgrid", Wikipedia, The Free Encyclopedia (revision 1384014039), https://en.wikipedia.org/w/index.php?title=Microgrid&oldid=1384014039 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Microgrid

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **microgrid** is a local electrical grid with defined electrical boundaries, acting as a single and controllable entity. It is able to operate in grid-connected and off-grid modes. Microgrids may be linked as a cluster or operated as **stand-alone** or **isolated microgrid** which only operates off-the-grid not be connected to a wider electric power system. Very small microgrids are sometimes called nanogrids when they serve a single building or load.

A grid-connected microgrid normally operates connected to and synchronous with the traditional wide area synchronous grid (macrogrid), but is able to disconnect from the interconnected grid and to function autonomously in "island mode" as technical or economic conditions dictate. In this way, they improve the security of supply within the microgrid cell, and can supply emergency power, changing between island and connected modes. This kind of grid is called an **islandable microgrid**.

One version of a microgrid implements control of small-scale distributed generation (DG) at a single house/small building level: the **nanogrid**. Modular open-source hardware DC nanogrids have been developed to provide solar photovoltaic power for any small-scale system such as an ambulance or even down the device level. Although DC systems generally are more efficient, nanogrids can also be AC to make them compatible with more mainstream devices.

A stand-alone microgrid has its own sources of electricity, supplemented with an energy storage system. They are used where power transmission and distribution from a major centralized energy source is too far and costly to operate. They offer an option for rural electrification in remote areas and on smaller geographical islands. A stand-alone microgrid can effectively integrate various sources of distributed generation, especially renewable energy sources (RES).

Control and protection are difficulties to microgrids, as all ancillary services for system stabilization must be generated within the microgrid and low short-circuit levels can be challenging for selective operation of the protection systems. An important feature is also to provide multiple useful energy needs, such as heating and cooling besides electricity, since this allows energy carrier substitution and increased energy efficiency due to waste heat utilization for heating, domestic hot water, and cooling purposes (cross sectoral energy usage).

## Definitions

The United States Department of Energy Microgrid Exchange Group defines a microgrid as "a group of interconnected loads and distributed energy resources within clearly defined electrical boundaries that acts as a single controllable entity with respect to the grid. A microgrid can connect and disconnect from the grid to enable it to operate in both grid-connected or island-mode."

The Berkeley Lab defines: "A microgrid consists of energy generation and energy storage that can power a building, campus, or community when not connected to the electric grid, e.g. in the event of a disaster." A microgrid that can be disconnected from the utility grid (at the 'point of common coupling' or PCC) is called an 'islandable microgrid'.

An EU research project describes a microgrid as comprising Low-Voltage (LV) distribution systems with distributed energy resources (DERs) (microturbines, fuel cells, photovoltaics (PV), etc.), storage devices (batteries, flywheels) energy storage system and flexible loads. Such systems can operate either connected or disconnected from the main grid. The operation of microsources in the network can provide benefits to the overall system performance, if managed and coordinated efficiently.

Electropedia defines a microgrid as a group of interconnected loads and distributed energy resources with defined electrical boundaries, which form a local electric power system at distribution voltage levels, meaning both low and medium voltage up to 35 kV. This cluster of associated consumer and producer nodes acts as a single controllable entity and is able to operate in either grid-connected or island mode.

Microgrid Knowledge defines a microgrid as a "self-sufficient energy system that serves a discrete geographic footprint, such as a college campus, hospital complex, business center or neighborhood."

A stand-alone microgrid or isolated microgrid, sometimes called an "island grid", only operates off-the-grid and cannot be connected to a wider electric power system. They are usually designed for geographical islands or for rural electrification. In many non-industrialized countries, microgrids that are used to provide access to electricity in previously unelectrified areas are often referred to as "mini grids". Nanogrids belong to a single home or building and the interconnection of multiple nanogrids forming a network (microgrid), facilitating the sharing of power between individual nanogrids.

### Campus environment/institutional microgrids

The focus of campus microgrids is aggregating existing on-site generation to support multiple loads located in a tight geographical area where an owner can easily manage them.

### Community microgrids

Community microgrids can serve thousands of customers and support the penetration of local energy (electricity, heating, and cooling).

### Remote off-grid microgrids

These microgrids are generally not designed or intended to connect to the macrogrid and instead operate in an island mode
at all times because of economic issues or geographical position. Typically, an "off-grid" microgrid is built in areas that are far distant from any transmission and distribution infrastructure and, therefore, have no connection to the utility grid. In some cases, off-grid microgrids are indeed incorporated into a national grid or 'macrogrid', a process that requires technical, regulatory and legal planning.

Because remote off-grid microgrids are often small and built from scratch, they have the potential to incorporate best practices from the global electricity sector and to incorporate and drive energy innovation. It is now common to see remote off-grid microgrids being largely powered by renewable energy and operated with customer-level smart controls, something that is not always easy to implement in the larger power sector because of incumbent interests and older, pre-existing infrastructure.

### Military base microgrids

These microgrids are being actively deployed with focus on both physical and cyber security for military facilities in order to assure reliable power without relying on the macrogrid.

## Topologies

Architectures are needed to manage the flow of energy from different types of sources into the electrical grid. Thus, the microgrid can be classified into three topologies:

### AC microgrid

Power sources with AC output are interfaced to AC bus through AC/AC converter which will transform the AC variable frequency and voltage to AC waveform with another frequency at another voltage. Whilst power sources with DC output use DC/AC converters for the connection to the AC bus.

### DC microgrid

In DC microgrid topology, power sources with DC output are connected to DC bus directly or by DC/DC converters. On the other hand, power sources with AC output are connected to the DC bus through AC/DC converter.

### Hybrid microgrid

The hybrid microgrid has topology for both power source AC and DC output. In addition, AC and DC buses are connected to each other through a bidirectional converter, allowing power to flow in both directions between the two buses.

_(Later sections of the original article were left out of this offline copy for length: Basic components, Advantages and challenges, Microgrid control, Examples.)_
