---
id: security/wp2-off-the-record-messaging-protocol
title: Off-the-Record Messaging protocol
category: security
tags: [off-the-record messaging protocol, off-the-record messaging, otr, security]
summary: Off-the-Record Messaging (OTR) is a cryptographic protocol that provides encryption for instant messaging conversations.
source: https://en.wikipedia.org/w/index.php?title=Off-the-Record_Messaging_protocol&oldid=1352449043
source_title: Wikipedia: Off-the-Record Messaging protocol
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Off-the-Record Messaging protocol", Wikipedia, The Free Encyclopedia (revision 1352449043), https://en.wikipedia.org/w/index.php?title=Off-the-Record_Messaging_protocol&oldid=1352449043 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Off-the-Record Messaging protocol

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Off-the-Record Messaging** (**OTR**) is a cryptographic protocol that provides encryption for instant messaging conversations. OTR uses a combination of AES symmetric-key algorithm with 128 bits key length, the Diffie–Hellman key exchange with 1536 bits group size, and the SHA-1 hash function. In addition to authentication and encryption, OTR provides forward secrecy and malleable encryption.

The primary motivation behind the protocol was providing deniable authentication for the conversation participants while keeping conversations confidential, like a private conversation in real life, or off the record in journalism sourcing. This is in contrast with cryptography tools that produce output which can be later used as a verifiable record of the communication event and the identities of the participants. The initial introductory paper was named "Off-the-Record Communication, or, Why Not to Use PGP".

The OTR protocol was designed by cryptographers Ian Goldberg and Nikita Borisov and released on 26 October 2004. They provide a client library to facilitate support for instant messaging client developers who want to implement the protocol. A Pidgin and Kopete plugin exists that allows OTR to be used over any IM protocol supported by Pidgin or Kopete, offering an auto-detection feature that starts the OTR session with the buddies that have it enabled, without interfering with regular, unencrypted conversations. Version 4 of the protocol has been in development since 2017 by a team led by Sofía Celi, and reviewed by Nik Unger and Ian Goldberg. This version aims to provide online and offline deniability, to update the cryptographic primitives, and to support out-of-order delivery and asynchronous communication.

According to classified NSA documents published in the *Der Spiegel* article on 28 December 2014, the NSA intercepted a conversation between two users, but messages could not be decrypted by the NSA because the users were using the OTR protocol.

## Implementation

In addition to providing encryption and authentication — features also provided by typical public-key cryptography suites, such as PGP, GnuPG, and X.509 (S/MIME) — OTR also offers some less common features:

**Forward secrecy Messages are only encrypted with temporary per-message AES keys, negotiated using the Diffie–Hellman key exchange protocol. The compromise of any long-lived cryptographic keys does not compromise any previous conversations, even if an attacker is in possession of ciphertexts.Deniable authentication Messages in a conversation do not have digital signatures, and after a conversation is complete, anyone is able to forge a message to appear to have come from one of the participants in the conversation, assuring that it is impossible to prove that a specific message came from a specific person. Within the conversation the recipient can be sure that a message is coming from the person they have identified.**

## Authentication

As of OTR 3.1, the protocol supports mutual authentication of users using a shared secret through the socialist millionaire protocol. This feature makes it possible for users to verify the identity of the remote party and avoid a man-in-the-middle attack without the inconvenience of manually comparing public key fingerprints through an outside channel.

## Limitations

Due to limitations of the protocol, OTR does not support multi-user group chat As of 2009 but it may be implemented in the future. As of version 3 of the protocol specification, during authenticated key exchanges an extra symmetric key is derived that can be used for secure communication (e.g., encrypted file transfers) over a different channel. Support for encrypted audio or video is not planned. (SRTP with ZRTP exists for that purpose.) A project to produce a protocol for multi-party off-the-record messaging (mpOTR) has been organized by Cryptocat, eQualitie, and other contributors, including Ian Goldberg.

Since the OTR protocol v3 (libotr 4.0.0) the plugin supports multiple OTR conversations with the same buddy who is logged in at multiple locations.

## Client support

These clients support Off-the-Record Messaging out of the box or via plug-ins.

- Adium (OS X)
- Another.IM
- BitlBee (cross-platform), since 3.0 (optional at compile time)
- Blink SIP client (OS X)
- CenterIM (Unix-like), since 4.22.2
- ChatSecure (iOS)
- CoyIM
- HexChat, for *nix versions, with a third-party plugin
- HexChat, with a third-party plugin
- Jitsi (cross-platform)
- Kopete (Unix-like)
- Miranda NG (Microsoft Windows), with a third-party plugin
- monocles chat, XMPP client supports OTR since 2022
- Mozilla Thunderbird, since 68
- Pidgin (cross-platform), with a plugin available from the OTR homepage
- Profanity, since 0.4.1
- Psi (cross-platform)
- Spark
- Zom Mobile Messenger (Android)
- Tkabber (cross-platform), since version 1.1
- WeeChat, with a third-party plugin
- climm (Unix-like), since (mICQ) 0.5.4
- irssi, since 1.2.0
