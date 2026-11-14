---
id: security/wp2-end-to-end-encryption
title: End-to-end encryption
category: security
tags: [end-to-end encryption, e2ee, security]
summary: End-to-end encryption (E2EE) is a method of implementing a secure communication system where only the sender and intended recipients can read the messages.
source: https://en.wikipedia.org/w/index.php?title=End-to-end_encryption&oldid=1384558232
source_title: Wikipedia: End-to-end encryption
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "End-to-end encryption", Wikipedia, The Free Encyclopedia (revision 1384558232), https://en.wikipedia.org/w/index.php?title=End-to-end_encryption&oldid=1384558232 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# End-to-end encryption

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**End-to-end encryption** (**E2EE**) is a method of implementing a secure communication system where only the sender and intended recipients can read the messages. No one else, including the system provider, telecom providers, Internet providers or malicious actors, can access the cryptographic keys needed to read or send messages.

End-to-end encryption prevents data from being read or secretly modified, except by the sender and intended recipients. In many applications, messages are relayed from a sender to some recipients by a service provider. In an E2EE-enabled service, messages are encrypted on the sender's device such that no third party, including the service provider, has the means to decrypt them. The recipients retrieve encrypted messages and decrypt them independently on their own devices. Since third parties cannot decrypt the data being communicated or stored, services with E2EE are better at protecting user data from data breaches and espionage.

Computer security experts, digital freedom organizations, and human rights activists advocate for the use of E2EE due to its security and privacy benefits, including its ability to resist mass surveillance. Popular messaging apps like WhatsApp, iMessage, Facebook Messenger and Signal use end-to-end encryption for chat messages, with some also supporting E2EE of voice and video calls. As of May 2025, WhatsApp is the most widely used E2EE messaging service, with over 3 billion users. Meanwhile, Signal with an estimated 70 million users, is regarded as the current gold standard in secure messaging by cryptographers, protestors and journalists.

Since end-to-end encrypted services cannot offer decrypted messages in response to government requests, the proliferation of E2EE has been met with controversy. Around the world, governments, law enforcement agencies and child protection groups have expressed concerns over its impact on criminal investigations. As of 2025, some governments have successfully passed legislation targeting E2EE, such as Australia's Telecommunications and Other Legislation Amendment Act (2018) and the Online Safety Act (2023) in the UK. Other attempts at restricting E2EE include the EARN IT Act in the US and the Child Sexual Abuse Regulation in the EU. Nevertheless, some government bodies such as the UK's Information Commissioner's Office and the US's Cybersecurity and Infrastructure Security Agency (CISA) have argued *for* the use of E2EE, with Jeff Greene of the CISA advising that "encryption is your friend" following the discovery of the Salt Typhoon espionage campaign in 2024.

## Definitions

End-to-end encryption is a means of ensuring the security of communications in applications like secure messaging. Under E2EE, messages are encrypted on the sender's device such that they can be decoded only by the final recipient's device. In many non-E2EE messaging systems, including email and many chat platforms, messages pass through intermediaries and are stored by a third-party service provider, from which they are retrieved by the recipient. Even if messages are encrypted, they are only encrypted 'in transit', and are thus accessible by the service provider. Server-side disk encryption is also distinct from E2EE because it does not prevent the service provider from viewing the information, as they have the encryption keys and can simply decrypt it.

The term "end-to-end encryption" originally only meant that the communication is never decrypted during its transport from the sender to the receiver. For example, around 2003, E2EE was proposed as an additional layer of encryption for GSM or TETRA, in addition to the existing radio encryption protecting the communication between the mobile device and the network infrastructure. This has been standardized by SFPG for TETRA. Note that in TETRA, the keys are generated by a Key Management Centre (KMC) or a Key Management Facility (KMF), not by the communicating users.

Later, around 2014, the meaning of "end-to-end encryption" started to evolve when WhatsApp encrypted a portion of its network, requiring that not only the communication stays encrypted during transport, but also that the provider of the communication service is not able to decrypt the communications—maliciously or when requested by law enforcement agencies. Similarly, messages must be undecryptable in transit by attackers through man-in-the-middle attacks. This new meaning is now the widely accepted one.

## Motivations

The lack of end-to-end encryption can allow service providers to easily provide search and other features, or to scan for illegal and unacceptable content. However, it also means that content can be read by anyone who has access to the data stored by the service provider, by design or via a backdoor. This can be a concern in many cases where privacy is important, such as in governmental and military communications, financial transactions, and when sensitive information such as health and biometric data are sent. If this content were shared without E2EE, a malicious actor or adversarial government could obtain it through unauthorized access or subpoenas targeted at the service provider.

E2EE alone does not guarantee privacy or security. For example, the data may be held unencrypted on the user's own device or accessed through their own app if their credentials are compromised.

## Modern implementations

### Messaging

In May 2026, Meta ended support for end-to-end encryption (E2EE) on Instagram, reversing a previous commitment to expand the technology across its messaging services. Meta said it was removing the feature because very few people were opting in to end-to-end encrypted messaging in Instagram DMs. The decision highlighted a conflict between digital privacy and online safety; while child protection organizations supported the change to better identify predatory behavior, privacy advocates argued that removing E2EE compromises user security.

As of 2025, messaging apps like Signal and WhatsApp are designed to exclusively use end-to-end encryption. Both Signal and WhatsApp use the Signal Protocol. Other messaging apps and protocols that support end-to-end encryption include Facebook Messenger, iMessage, Telegram, Matrix and Keybase. Although Telegram supports end-to-end encryption, it has been criticized for not enabling it by default, instead supporting E2EE through opt-in "secret chats". As of 2026, Telegram does not support E2EE for group chats and E2EE is not supported on its desktop clients.

Facebook Messenger first introduced optional end-to-end encryption in 2016 through its "Secret Conversations" feature. In 2022, after controversy over the use of Facebook Messenger messages in a Nebraska criminal case involving an abortion, Meta expanded testing of end-to-end encryption in Messenger. Writing for *Wired*, technologist Albert Fox Cahn criticized Messenger's approach to end-to-end encryption, which required the user to opt into E2EE for each conversation and split the message thread into two chats which were easy for users to confuse. In December 2023, Facebook announced plans to enable end-to-end encryption by default despite pressure from British law enforcement agencies.

As of 2016, many server-based communications systems did not include end-to-end encryption. These systems can only guarantee the protection of communications between clients and servers, meaning that users have to trust the third parties who are running the servers with the sensitive content. End-to-end encryption is regarded as safer because it reduces the number of parties who might be able to interfere or break the encryption. In the case of instant messaging, users may use a third-party client or plugin to implement an end-to-end encryption scheme over an otherwise non-E2EE protocol.

### Audio and video conferencing

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Modern implementations, Law enforcement and regulation, Challenges.)_
