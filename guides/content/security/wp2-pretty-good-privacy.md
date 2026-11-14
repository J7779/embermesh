---
id: security/wp2-pretty-good-privacy
title: Pretty Good Privacy
category: security
tags: [pretty good privacy, pgp, security]
summary: Pretty Good Privacy (PGP) is an encryption program that provides cryptographic privacy and authentication for data communication.
source: https://en.wikipedia.org/w/index.php?title=Pretty_Good_Privacy&oldid=1384563207
source_title: Wikipedia: Pretty Good Privacy
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Pretty Good Privacy", Wikipedia, The Free Encyclopedia (revision 1384563207), https://en.wikipedia.org/w/index.php?title=Pretty_Good_Privacy&oldid=1384563207 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Pretty Good Privacy

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Pretty Good Privacy** (**PGP**) is an encryption program that provides cryptographic privacy and authentication for data communication. PGP is used for signing, encrypting, and decrypting texts, e-mails, files, directories, and whole disk partitions and to increase the security of e-mail communications. Phil Zimmermann developed PGP in 1991.

The PGP software follows the OpenPGP standard in the now historical version.
The software has not been kept up to date with developments in OpenPGP standardization. While the product is still being sold, its development appears to have stalled sometime around 2010: The current user's guide of "PGP Encryption Desktop for Windows" only mentions RSA and Diffie-Hellman key material, and lacks any mention of ECC cryptography (which has been introduced to OpenPGP in 2012 by ).
The product also has no support for the updated formats in.

Modern versions of PGP are interoperable with GnuPG and other OpenPGP-compliant systems, when using cryptographic algorithms that both sides of a communication can handle. This algorithmic intersection has been shrinking with time.

## Design

Most aspects of PGP design apply more broadly to OpenPGP and are described at.

## Security quality

To the best of publicly available information, there is no known method which will allow a person or group to break PGP encryption by cryptographic or computational means. Indeed, in 1995, cryptographer Bruce Schneier characterized an early version as being "the closest you're likely to get to military-grade encryption". Early versions of PGP have been found to have theoretical vulnerabilities and so current versions are recommended. In addition to protecting data in transit over a network, PGP encryption can also be used to protect data in long-term data storage such as disk files. These long-term storage options are also known as data at rest, i.e. data stored, not in transit.

The cryptographic security of PGP encryption depends on the assumption that the algorithms used are unbreakable by direct cryptanalysis with current equipment and techniques.

In the original version, the RSA algorithm was used to encrypt session keys. RSA's security depends upon the one-way function nature of mathematical integer factoring. Similarly, the symmetric key algorithm used in PGP version 2 was IDEA, which might at some point in the future be found to have previously undetected cryptanalytic flaws. Specific instances of current PGP or IDEA insecurities (if they exist) are not publicly known. As current versions of PGP have added additional encryption algorithms, their cryptographic vulnerability varies with the algorithm used. However, none of the algorithms in current use are publicly known to have cryptanalytic weaknesses.

New versions of PGP are released periodically and vulnerabilities fixed by developers as they come to light. Any agency wanting to read PGP messages would probably use easier means than standard cryptanalysis, e.g. rubber-hose cryptanalysis or black-bag cryptanalysis (e.g. installing some form of trojan horse or keystroke logging software or hardware on the target computer to capture encrypted keyrings and their passwords). The FBI has already used this attack against PGP in its investigations. However, any such vulnerabilities apply not just to PGP but to any conventional encryption software.

In 2003, an incident involving seized Psion PDAs belonging to members of the Red Brigade indicated that neither the Italian police nor the FBI were able to decrypt PGP-encrypted files stored on them.

A second incident in December 2006, (see *In re Boucher*), involving US customs agents who seized a laptop PC that allegedly contained child pornography, indicates that US government agencies find it "nearly impossible" to access PGP-encrypted files. Additionally, a magistrate judge ruling on the case in November 2007 has stated that forcing the suspect to reveal his PGP passphrase would violate his Fifth Amendment rights i.e. a suspect's constitutional right not to incriminate himself. The Fifth Amendment issue was opened again as the government appealed the case, after which a federal district judge ordered the defendant to provide the key.

Evidence suggests that, British police investigators are unable to break PGP, so instead have resorted to using RIPA legislation to demand the passwords/keys. In November 2009 a British citizen was convicted under RIPA legislation and jailed for nine months for refusing to provide police investigators with encryption keys to PGP-encrypted files.

## PGP Corporation encryption applications

*This section describes commercial programs available from PGP Corporation. For information on other programs compatible with the OpenPGP specification, see External links below.*

While originally used primarily for encrypting the contents of e-mail messages and attachments from a desktop client, PGP products have been diversified since 2002 into a set of encryption applications that can be managed by an optional central policy server. PGP encryption applications include e-mails and attachments, digital signatures, full disk encryption, file and folder security, protection for IM sessions, batch file transfer encryption, and protection for files and folders stored on network servers and, more recently, encrypted or signed HTTP request/responses by means of a client-side (Buanzo's Enigform) and a server-side (also Buanzo's mod_openpgp) module. There is also a WordPress plugin available, called wp-enigform-authentication, that takes advantage of the session management features of Enigform with mod_openpgp.

The PGP Desktop 9.x family includes PGP Desktop Email, PGP Whole Disk Encryption, and PGP NetShare. Additionally, a number of Desktop bundles are also available. Depending on the application, the products feature desktop e-mail, digital signatures, IM security, whole disk encryption, file, and folder security, encrypted self-extracting archives, and secure shredding of deleted files. Capabilities are licensed in different ways depending on the features required.

The PGP Universal Server 2.x management console handles centralized deployment, security policy, policy enforcement, key management, and reporting. It is used for automated e-mail encryption in the gateway and manages PGP Desktop 9.x clients. In addition to its local keyserver, PGP Universal Server works with the PGP public keyserver—called the PGP Global Directory—to find recipient keys. It has the capability of delivering e-mail securely when no recipient key is found via a secure HTTPS browser session.

With PGP Desktop 9.x managed by PGP Universal Server 2.x, first released in 2005, all PGP encryption applications are based on a new proxy-based architecture. These newer versions of PGP software eliminate the use of e-mail plug-ins and insulate the user from changes to other desktop applications. All desktop and server operations are now based on security policies and operate in an automated fashion. The PGP Universal server automates the creation, management, and expiration of keys, sharing these keys among all PGP encryption applications.

The Symantec PGP platform has now undergone a rename. PGP Desktop is now known as Symantec Encryption Desktop (SED), and the PGP Universal Server is now known as Symantec Encryption Management Server (SEMS). The current shipping versions are Symantec Encryption Desktop 10.3.0 (Windows and macOS platforms) and Symantec Encryption Server 3.3.2.

Also available are PGP Command-Line, which enables command line-based encryption and signing of information for storage, transfer, and backup, as well as the PGP Support Package for BlackBerry which enables RIM BlackBerry devices to enjoy sender-to-recipient messaging encryption.

New versions of PGP applications use both OpenPGP and the S/MIME, allowing communications with any user of a NIST specified standard.
