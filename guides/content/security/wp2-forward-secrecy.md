---
id: security/wp2-forward-secrecy
title: Forward secrecy
category: security
tags: [forward secrecy, perfect forward secrecy, pfs, security]
summary: In cryptography, forward secrecy (FS), also known as perfect forward secrecy (PFS), is a feature of specific key-agreement protocols that gives assurances that session keys will not be compromised even if long-term secrets used in the session key exchange are compromised, limiting damage.
source: https://en.wikipedia.org/w/index.php?title=Forward_secrecy&oldid=1374258328
source_title: Wikipedia: Forward secrecy
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Forward secrecy", Wikipedia, The Free Encyclopedia (revision 1374258328), https://en.wikipedia.org/w/index.php?title=Forward_secrecy&oldid=1374258328 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Forward secrecy

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In cryptography, **forward secrecy** (**FS**), also known as **perfect forward secrecy** (**PFS**), is a feature of specific key-agreement protocols that gives assurances that session keys will not be compromised even if long-term secrets used in the session key exchange are compromised, limiting damage. For TLS, the long-term secret is typically the private key of the server. Forward secrecy protects past sessions against future compromises of keys or passwords. By generating a unique session key for every session a user initiates, the compromise of a single session key will not affect any data other than that exchanged in the specific session protected by that particular key. This by itself is not sufficient for forward secrecy, which additionally requires that a long-term secret compromise does not affect the security of past session keys.

Forward secrecy protects data on the transport layer of a network that uses common transport layer security protocols, including OpenSSL, when its long-term secret keys are compromised, as with the Heartbleed security bug. If forward secrecy is used, encrypted communications and sessions recorded in the past cannot be retrieved and decrypted should long-term secret keys or passwords be compromised in the future, even if the adversary actively interfered, for example via a man-in-the-middle (MITM) attack.

The value of forward secrecy is that it protects past communication. This reduces the motivation for attackers to compromise keys. For instance, if an attacker learns a long-term key, but the compromise is detected and the long-term key is revoked and updated, relatively little information is leaked in a forward secure system.

The value of forward secrecy depends on the assumed capabilities of an adversary. Forward secrecy has value if an adversary is assumed to be able to obtain secret keys from a device (read access) but is either detected or unable to modify the way session keys are generated in the device (full compromise). In some cases an adversary who can read long-term keys from a device may also be able to modify the functioning of the session key generator, as in the backdoored Dual Elliptic Curve Deterministic Random Bit Generator. If an adversary can make the random number generator predictable, then past traffic will be protected but all future traffic will be compromised.

The value of forward secrecy is limited not only by the assumption that an adversary will attack a server by only stealing keys and not modifying the random number generator used by the server but it is also limited by the assumption that the adversary will only passively collect traffic on the communications link and not be active using a man-in-the-middle attack. Forward secrecy typically uses an ephemeral Diffie–Hellman key exchange to prevent reading past traffic. The ephemeral Diffie–Hellman key exchange is often signed by the server using a static signing key. If an adversary can steal (or obtain through a court order) this static (long term) signing key, the adversary can masquerade as the server to the client and as the client to the server and implement a classic man-in-the-middle attack.

## Definition

An encryption system has the property of forward secrecy if plain-text (decrypted) inspection of the data exchange that occurs during key agreement phase of session initiation does not reveal the key that was used to encrypt the remainder of the session.

## Example

The following is a hypothetical example of a simple instant messaging protocol that employs forward secrecy:

1. Alice and Bob each generate a pair of long-term, asymmetric public and private keys, then verify public-key fingerprints in person or over an already-authenticated channel. Verification establishes with confidence that the claimed owner of a public key is the actual owner.
1. Alice and Bob use a key exchange algorithm such as Diffie–Hellman, to securely agree on an ephemeral session key. They use the keys from step 1 only to authenticate one another during this process.
1. Alice sends Bob a message, encrypting it with a symmetric cipher using the session key negotiated in step 2.
1. Bob decrypts Alice's message using the key negotiated in step 2.
1. The process repeats for each new message sent, starting from step 2 (and switching Alice and Bob's roles as sender/receiver as appropriate). Step 1 is never repeated.
Forward secrecy (achieved by generating new session keys for each message) ensures that past communications cannot be decrypted if one of the keys generated in an iteration of step 2 is compromised, since such a key is only used to encrypt a single message. Forward secrecy also ensures that past communications cannot be decrypted if the long-term private keys from step 1 are compromised. However, masquerading as Alice or Bob would be possible going forward if this occurred, possibly compromising all future messages.

## Attacks

Forward secrecy is designed to prevent the compromise of a long-term secret key from affecting the confidentiality of past conversations. However, forward secrecy cannot defend against a successful cryptanalysis of the underlying ciphers being used, since a cryptanalysis consists of finding a way to decrypt an encrypted message without the key, and forward secrecy only protects keys, not the ciphers themselves. A patient attacker can capture a conversation whose confidentiality is protected through the use of public-key cryptography and wait until the underlying cipher is broken (e.g. large quantum computers could be created which allow the discrete logarithm problem to be computed quickly), a.k.a. harvest now, decrypt later attacks. This would allow the recovery of old plaintexts even in a system employing forward secrecy.

Non-interactive forward-secure key exchange protocols face additional threats that are not relevant to interactive protocols. In a *message suppression* attack, an attacker in control of the network may itself store messages while preventing them from reaching the intended recipient; as the messages are never received, the corresponding private keys may not be destroyed or punctured, so a compromise of the private key can lead to successful decryption. Proactively retiring private keys on a schedule mitigates, but does not eliminate, this attack. In a *malicious key exhaustion* attack, the attacker sends many messages to the recipient and exhausts the private key material, forcing a protocol to choose between failing closed (and enabling denial of service attacks) or failing open (and giving up some amount of forward secrecy).

## Weak perfect forward secrecy

Weak perfect forward secrecy (Wpfs) is the weaker property whereby when agents' long-term keys are compromised, the secrecy of previously established session keys is guaranteed, but only for sessions in which the adversary did not actively interfere. This new notion, and the distinction between this and forward secrecy, was introduced by Hugo Krawczyk in 2005.
This weaker definition implicitly requires that full (perfect) forward secrecy maintains the secrecy of previously established session keys even in sessions where the adversary *did* actively interfere or attempted to act as a man in the middle.

_(Later sections of the original article were left out of this offline copy for length: Non-interactive forward secrecy, Protocols, Use.)_
