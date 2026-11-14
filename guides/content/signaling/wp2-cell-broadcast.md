---
id: signaling/wp2-cell-broadcast
title: Cell Broadcast
category: signaling
tags: [cell broadcast, signals]
summary: Cell Broadcast (CB) is a method of simultaneously sending short messages to multiple mobile telephone users in a defined area.
source: https://en.wikipedia.org/w/index.php?title=Cell_Broadcast&oldid=1382411024
source_title: Wikipedia: Cell Broadcast
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Cell Broadcast", Wikipedia, The Free Encyclopedia (revision 1382411024), https://en.wikipedia.org/w/index.php?title=Cell_Broadcast&oldid=1382411024 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Cell Broadcast

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Cell Broadcast** (**CB**) is a method of simultaneously sending short messages to multiple mobile telephone users in a defined area. It is defined by the ETSI's GSM committee and 3GPP and is part of the 2G, 3G, 4G and 5G standards. It is also known as Short Message Service-Cell Broadcast (SMS-CB or CB SMS).

Cell Broadcast is different from the regular Short Message Service (which is also called *Short Message Service-Point to Point* / SMS-PP to distinguish it). Cell Broadcast is a one-to-many geo-targeted and geo-fenced messaging service, which typically targets all handsets connected to a specific network cell. Cell Broadcast technology is widely used for public warning systems.

## Technology

One Cell Broadcast message can reach a large number of telephones at once. Cell Broadcast messages are directed to specific radio cells of a mobile phone network, rather than to a specific telephone. The latest generation of Cell Broadcast Systems (CBS) can send to the whole mobile network (e.g. 1,000,000 cells) in less than 10 seconds, reaching millions of mobile subscribers at the same time. A Cell Broadcast message is an unconfirmed push service, meaning that the originators of the messages do not know who has received the message, allowing for services based on anonymity. Cell Broadcast is compliant with the latest EU General Data Protection Regulation (GDPR) as mobile phone numbers are not required by CB. The originator (alerting authority) of the Cell Broadcast message can request the success rate of a message. In such a case the Cell Broadcast System will respond with the number of addressed cells and the number of cells that have broadcast the Cell Broadcast (alert) message.

Each radio cell covers a certain geographic area, typically a few kilometers in diameter, so by only sending the Cell Broadcast message to specific radio cells, the broadcast can be limited to a specific area (geotargeting). This is useful for messages that are only relevant in a specific area, such as flood warnings.

The CB message parameters contain the broadcasting schedule. If the start-time is left open, the CBC system will assume an immediate start, which will be the case for Public Warning messages. If the end-time is left open, the message will be repeated indefinitely. A subsequent cancel message shall be used to stop this message. The repetition rate can be set between 2 seconds and to values beyond 30 minutes. Each repeated CB message will have the same message identifier (indicating the source of the message), and the same serial number. Using this information, the mobile telephone is able to identify and ignore broadcasts of already received messages.

A Cell Broadcast message page is composed of 82 octets, which, using the default character set, can encode 93 characters. Up to 15 of these pages may be concatenated to form a Cell Broadcast message (hence maximum length of one Cell Broadcast message is therefore 1395 characters).

A Cell Broadcast Centre (CBC), a system which is the source of SMS-CB message, is connected to a Base Station Controller (BSC) in GSM networks, to a Radio Network Controller (RNC) in UMTS networks, to a Mobility Management Entity (MME) in LTE networks or to a core Access and Mobility management Function (AMF) in 5G networks.

The technical implementation of the Cell Broadcast service is described in the 3GPP specification TS 23.041

- The 2G-CBC (BSC) interface is described in 3GPP standard TS 48.049; however, non-standard implementations exist.
- The 3G-CBC (RNC) interface is described in 3GPP standard TS 25.419.
- The 4G-CBC (MME) interface is described in 3GPP standard TS 29.168.
- The 5G-CBC (AMF) interface is described in 3GPP standard TS 29.518.

A CBC sends CB messages, a list of cells where messages are to be broadcast, and the requested repetition rate and number of times they shall be broadcast to the BSC/RNC/MME/AMF. The BSC's/RNC's/MME/AMF responsibility is to deliver the CB messages to the base stations (BTSs), Node Bs, ENodeBs and gNodeBs which handle the requested cells.

## Public warning systems

Cell broadcast is not affected by traffic load; therefore, it is very suitable during a disaster when load spikes of data (social media and mobile apps), regular SMS and voice calls usage (mass call events) tend to significantly congest mobile networks, as multiple events have shown.

Public warning systems, otherwise known as emergency alert systems, implemented through cell broadcast technology vary by country, but are broadly the same. Technical standards are outlined in the 3GPP TS 23.041 standard. Large implementations mentioned in 3GPP standards are Wireless Emergency Alerts (CMAS) in the United States and EU-Alert in Europe (set out in ETSI standards, but national implementation varies). Alerts can be geo-targeted, when only phones in a defined geographical area are set to receive an alert. When an alert is received, a notification is shown in a unique format and a dedicated sound is played even if the phone is set to silent: a of 853 Hz and 960 Hz sine waves, as prescribed by both WEA (CMAS) and ETSI standards. Cell Broadcast emergency alerts can be broadcast in a local language and an additional language, which will be displayed depending on the user's device language setting. Most phone manufacturers adhere to these standards but have slightly different user interfaces. Similar to emergency calls, devices do not usually need a SIM card to receive alerts.
Emergency alerts in most implementations of cell broadcast have distinct alert categories or levels, using a message identifier outlined in 3GPP standards. The alert category or level is defined by the severity of the warning, e.g. threat to life, imminent danger or advisory message. Depending on national implementation, users may be able to opt-out of receiving lower level alerts. However, the highest level of alert will usually always be displayed on a user's device.

Below is a comparison table on alert categories/levels across systems (based on the common 3GPP message identifiers):

| 3GPP Message Identifier Hex code | Wireless Emergency Alerts (CMAS) | EU-Alert (ETSI) | Opt-out |
|---|---|---|---|
| 1112 | National Alert (Presidential Alert) | EU-Alert Level 1 | No |
| 1113, 1114, 1120, 1121 | Extreme Alert | EU-Alert Level 2 | Yes |
| 1115-1119, 111A, 1122–1127 | Severe Alert | EU-Alert Level 3 | Yes |
| 112C | Public Safety Alert | EU-Alert Level 4 | Yes |
| 111B | Amber Alert | EU-Amber | Yes |
| 1900 | - | EU-Info (depreciated to Level 4) | Yes |
| 111C | Required Monthly Test | EU-Monthly Test | Yes |
| 112E | State/Local Test | EU-Test | Yes |
| 111D | Exercise | EU-Exercise | Yes |

When roaming, if the user's home carrier supports Cell Broadcast emergency alerts, alerts will be displayed if the category/level of alert is enabled and equivalent to their home carrier's system.

Cell Broadcast messages can use a CAP (Common Alerting Protocol) message as an input as specified by OASIS or the Wireless Emergency Alerts (WEA) C-interface protocol, which has been specified jointly by the Alliance for Telecommunications Industry Solutions (ATIS) and the Telecommunications Industry Association (TIA).

Advantages of using Cell Broadcast for Public warning are:

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Public warning systems, Cell Broadcast adoption rate, Public warning implementations.)_
