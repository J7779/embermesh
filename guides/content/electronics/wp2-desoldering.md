---
id: electronics/wp2-desoldering
title: Desoldering
category: electronics
tags: [desoldering, electronics]
summary: In electronics, desoldering is the removal of soldered components from a circuit board for troubleshooting, repair, replacement, and salvage.
source: https://en.wikipedia.org/w/index.php?title=Desoldering&oldid=1378171373
source_title: Wikipedia: Desoldering
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Desoldering", Wikipedia, The Free Encyclopedia (revision 1378171373), https://en.wikipedia.org/w/index.php?title=Desoldering&oldid=1378171373 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Desoldering

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electronics, **desoldering** is the removal of soldered components from a circuit board for troubleshooting, repair, replacement, and salvage. Desoldering involves breaking the soldered connection between component and board, typically by reflowing and removing the solder, allowing the component to be removed and potentially replaced if needed.

Specific tools and techniques used for desoldering a component vary depending on the type of component (through-hole or surface-mount) and on whether the component needs to be recovered with or without damage. These include the use of soldering irons or heat guns to reflow the solder, and vacuum pumps or desoldering braid to remove the melted solder.

## Tools

Desoldering tools and materials include the following:

- Solder wick
- Heat guns, also called hot air guns
- Desoldering pump
- Removal alloys
- Removal fluxes
- Heated soldering tweezers
- Various picks and tweezers for tasks such as pulling at, holding, removing, and scraping components.
- Vacuum and pressure pumps with specialized heater tips and nozzles
- Rework stations, used to repair printed circuit board assemblies that fail factory test.
Terminology is not totally standardised. Anything with a base unit with provision to maintain a stable temperature, pump air in either direction, etc., is often called a "station" (preceded by rework, soldering, desoldering, hot air); one, or sometimes more, tools may be connected to a station, e.g., a rework station may accommodate a soldering iron and hot air head. A soldering iron with a hollow tip and a spring-, bulb-, or electrically operated suction pump may be called a desoldering iron. Terms such as "suction pen" may be used; the meaning is usually clear from the context.

### Pumps

Electrically operated pumps are used for several purposes in conjunction with a hand-held head connected by a tube.

Suction pumps are used to suck away molten solder, leaving previously joined terminals disconnected. They are primarily used to release through-hole connections from a PCB. The desoldering head must be designed so that the extracted solder does not solidify so as to obstruct it, or enter the pump, and can be removed and discarded easily. It is not possible to remove a multi-pin part by melting solder on the pins sequentially, as one joint will solidify as the next is melted; pumps and solder wick are among methods to remove solder from all joints, leaving the part free to be removed.

Suction pumps are also used with a suction head appropriate for each part to pick up and remove tiny surface mount devices once solder has melted, and to place parts.

Hot air pumps blow air hot enough to melt all the solder around a small surface mounted part, and can be used for soldering parts in place, and for desoldering followed by removal before the solder solidifies by a vacuum pump or with tweezers. Hot air has a tendency to oxidise metals; a non-oxidising gas, usually nitrogen, can be used instead of air, at increased cost of equipment and consumables.

#### Desoldering pump

A **desoldering pump**, colloquially known as a **solder sucker**, is a manually operated device which is used to remove solder from a printed circuit board. There are two types: the *plunger* style and *bulb* style. (An electrically operated pump for this purpose would usually be called a vacuum pump.)

The plunger type has a cylinder with a spring-loaded piston which is pushed down and locks into place. When triggered by pressing a button, the piston springs up, creating suction that sucks the solder off the soldered connection. The bulb type creates suction by squeezing and releasing a rubber bulb.

The pump is applied to a heated solder connection, then operated to suck the solder away.

### Desoldering braid

**Desoldering braid**, also known as **desoldering wick** or **solder wick**, is finely braided 18 to 42 AWG copper wire coated with rosin flux, usually supplied on a roll.

The end of a length of braid is placed over the soldered connections of a component being removed. The connections are heated with a soldering iron until the solder melts and is wicked into the braid by capillary action. The braid is removed while the solder is still molten, its used section cut off and discarded when cool. Short lengths of cut braid will prevent heat being carried away by the braid instead of heating the joint.

## Technique

Desoldering requires application of heat to the solder joint and removing the molten solder so that the joint may be separated. Desoldering may be required to replace a defective component, to alter an existing circuit, or to salvage components for re-use. Use of too high a temperature or heating for too long may damage components or destroy the bond between a printed circuit trace and the board substrate. Techniques are different for through-hole and surface-mounted components.

### Through-hole

A component with one or two connections to the PCB can usually be removed by heating one joint, pulling out an end of the component while the solder is molten (bending the other lead to do so), and repeating for the second joint. Solder filling the hole can be removed with a pump or with a pointed object made of a material which solder does not wet, such as stainless steel or wood.

If a multi-pin component need not be salvaged, it is often possible to cut the pins, then remove the residual ends one by one.

Components with more connections cannot be removed intact in the way described above unless the wire leads are long and flexible enough to be pulled out one by one. For a component such as a Dual-Inline Package (DIP), the pins are too short to pull out, and solder melted on one joint will solidify before another can be melted. A technique sometimes used is the use of a large soldering-iron tip designed to melt the solder on all pins at once; different tips are required for different packages. The component is removed while the solder is molten, most easily by a spring-loaded puller attached to it before heating.

Otherwise all joints must be freed from solder before the component can be removed. Each joint must be heated and the solder removed from it while molten using a vacuum pump, manual desoldering pump, or desoldering braid.

For through-hole technology on double-sided or multi-layer boards, special care must be taken not to remove the via connecting the layers, as this will ruin the entire board. Hard pulling on a lead which is not entirely free of solder (or with solder not thoroughly molten in the case of a soldering iron tip heating all pins) may pull out a via.

To remove and recover all components, both through-hole and surface-mount, from a board which itself is usually no longer needed, a flame or hot air gun can be used to rapidly heat all parts so they can be pulled off. Parts may be damaged, and toxic fumes emitted, if excessive temperature or prolonged heating is used.

### Surface mount

If they do not need to be re-used, some surface-mount components can be removed by cutting their leads and desoldering the remnants with a soldering iron.

Otherwise, surface-mount components must be removed by heating the entire component to a temperature sufficient to melt the solder used, but not high or prolonged enough to damage the component. For most purposes, a temperature not exceeding 260 °C for a time not exceeding 10 seconds is acceptable.

The entire board may be preheated to a temperature that all components can withstand indefinitely. Then localised heat is applied to the component to remove, with less heating required than from cold. Most frequently, a hot air (or hot gas) gun, with a nozzle of appropriate size and shape, is used to heat the component, with nearby components shielded from the heat if necessary, followed by removal with tweezers or a vacuum tool. Removal of multi-pin components with a soldering iron and solder removal tools is impractical, as the solder between the component and the pads remains in place, unlike solder which can be removed from a hole.

Hot air (or gas) may be applied with tools ranging from some portable gas soldering irons such as the Weller Portasol Professional which can be fitted with a narrow hot-air nozzle, set to a temperature not controlled but approximately correct, to an industrial rework station with many facilities including hot-gas blowing, vacuum part holding, soldering iron head, and nozzles and fitting specific to particular component packages.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Technique.)_
