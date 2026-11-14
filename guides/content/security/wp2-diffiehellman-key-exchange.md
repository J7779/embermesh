---
id: security/wp2-diffiehellman-key-exchange
title: Diffie–Hellman key exchange
category: security
tags: [diffie–hellman key exchange, diffie–hellman, key exchange, security]
summary: Diffie–Hellman (DH) key exchange is a mathematical method of securely generating a symmetric cryptographic key over a public channel.
source: https://en.wikipedia.org/w/index.php?title=Diffie%E2%80%93Hellman_key_exchange&oldid=1385452357
source_title: Wikipedia: Diffie–Hellman key exchange
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Diffie–Hellman key exchange", Wikipedia, The Free Encyclopedia (revision 1385452357), https://en.wikipedia.org/w/index.php?title=Diffie%E2%80%93Hellman_key_exchange&oldid=1385452357 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Diffie–Hellman key exchange

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Diffie–Hellman** (**DH**) **key exchange** is a mathematical method of securely generating a symmetric cryptographic key over a public channel. It is named after Whitfield Diffie and Martin Hellman who published it in 1976. DH is one of the earliest practical examples of key exchange implemented within the field of cryptography.

Traditionally, secure encrypted communication between two parties required that they first exchange keys by some secure physical means, such as paper key lists transported by a trusted courier. By contrast, the Diffie–Hellman key exchange method allows two parties that have no prior knowledge of each other to jointly establish a shared secret key over an insecure channel. This key can then be used to encrypt subsequent communications using a symmetric-key cipher.

The scheme was published by Whitfield Diffie and Martin Hellman in 1976 and named as one of Ralph Merkle's original public key protocols in 1978. The method was followed shortly after by the RSA cryptosystem, an implementation of public-key cryptography using asymmetric algorithms. In 1997 it was revealed that James H. Ellis, Clifford Cocks, and Malcolm J. Williamson of GCHQ, the British signals intelligence agency, had previously shown in 1969 how public-key cryptography could be achieved.

Diffie–Hellman is used to secure a variety of Internet services. However, research published in October 2015 suggests that the parameters in use for many DH Internet applications at that time were not strong enough to prevent compromise by very well-funded attackers, such as the security services of some countries.

Although Diffie–Hellman key exchange itself is a non-authenticated key-agreement protocol, it provides the basis for a variety of authenticated protocols, and is used to provide forward secrecy in Transport Layer Security's ephemeral modes (referred to as EDH or DHE depending on the cipher suite). Forward secrecy results from the use of ephemeral keys: the private keys are discarded once key agreement is complete, making them safe from later compromise. Ephemeral keys are practical because it is computationally cheap to create public-private key pairs suitable for use with Diffie-Hellman exchange.

Expired US patent 4200770 from 1977 describes the now public-domain algorithm. It credits Hellman, Diffie, and Merkle as inventors.

## Description

### General overview

Diffie–Hellman key exchange establishes a shared secret between two parties that can be used for secret communication for exchanging data over a public network. An analogy illustrates the concept of public key exchange by using colors instead of very large numbers:

The process begins by having the two parties, Alice and Bob, publicly agree on an arbitrary starting color that does not need to be kept secret. In this example, the color is yellow. Each person also selects a secret color that they keep to themselves – in this case, red and cyan. The crucial part of the process is that Alice and Bob each mix their own secret color together with their mutually shared color, resulting in orange-tan and light-blue mixtures respectively, and then publicly exchange the two mixed colors. Finally, each of them mixes the color they received from the partner with their own private color. The result is a final color mixture (yellow-brown in this case) that is identical to their partner's final color mixture.

If a third party listened to the exchange, they would only know the common color (yellow) and the first mixed colors (orange-tan and light-blue), but it would be very hard for them to find out the final secret color (yellow-brown). Bringing the analogy back to a real-life exchange using large numbers rather than colors, this determination is computationally expensive; it is impossible to compute in a practical amount of time even for modern supercomputers.

### Cryptographic explanation

The simplest and the original implementation, later formalized as **Finite Field Diffie–Hellman** in RFC 7919, of the protocol uses the multiplicative group of integers modulo *p*, where *p* is prime, and *g* is a primitive root modulo *p*. To guard against potential vulnerabilities, it is recommended to use prime numbers of at least 2048 bits in length. This increases the difficulty for an adversary attempting to compute the discrete logarithm and compromise the shared secret. These two values are chosen in this way to ensure that the resulting shared secret can take on any value from 1 to *p* − 1. Here is an example of the protocol, with non-secret values in, and secret values in.

1. Alice and Bob publicly agree to use a modulus = and base = (which is a primitive root modulo 23).
1. Alice chooses a secret integer = 4, then sends Bob = mod
 - = mod = (in this example both and have the same value 4, but this is usually not the case)
1. Bob chooses a secret integer = 3, then sends Alice = mod
 - = mod =
1. Alice computes = mod
 - = mod =
1. Bob computes = mod
 - = mod =
1. Alice and Bob now share a secret (the number 18).

Both Alice and Bob have arrived at the same values because under mod *p*,
colorBlueA^colorRedbbmod colorBluep = colorBlueg^colorRedabbmod colorBluep = colorBlueg^colorRedbabmod colorBluep = colorBlueB^colorRedabmod colorBluep

More specifically,
(colorBlueg^colorRedabmod colorBluep)^colorRedbbmod colorBluep = (colorBlueg^colorRedbbmod colorBluep)^colorRedabmod colorBluep

Only *a* and *b* are kept secret. All the other values – *p*, *g*, *g^a* mod *p*, and *g^b* mod *p* – are sent in the clear. The strength of the scheme comes from the fact that *g^ab* mod *p* = *g^ba* mod *p* take extremely long times to compute by any known classical algorithm just from the knowledge of *p*, *g*, *g^a* mod *p*, and *g^b* mod *p*. Such a function that is easy to compute but hard to invert is called a one-way function. Once Alice and Bob compute the shared secret they can use it as an encryption key, known only to them, for sending messages across the same open communications channel.

Of course, much larger values of *a*, *b*, and *p* would be needed to make this example secure, since there are only 23 possible results of *n* mod 23. However, if *p* is a prime of at least 600 digits, then even the fastest modern computers using the fastest known algorithm cannot find *a* given only *g*, *p* and *g^a* mod *p*. Such a problem is called the discrete logarithm problem. The computation of *g^a* mod *p* is known as modular exponentiation and can be done efficiently even for large numbers.
Note that *g* does not need to be large at all, and in practice is usually a small integer (like 2, 3, ...).

### Secrecy chart

The chart below depicts who knows what, again with non-secret values in, and secret values in. Here Eve is an eavesdropper – she watches what is sent between Alice and Bob, but she does not alter the contents of their communications.
-, public (primitive root) base, known to Alice, Bob, and Eve. =
-, public (prime) modulus, known to Alice, Bob, and Eve. =
-, Alice's private key, known only to Alice. =
-, Bob's private key known only to Bob. =
-, Alice's public key, known to Alice, Bob, and Eve. = mod =
-, Bob's public key, known to Alice, Bob, and Eve. = mod =

| | | |
|---|---|---|
| **Alice**; / Known / Unknown /; /---/---/; / = / /; / = / /; / = / /; / = mod / /; / = mod = / /; / = / /; / = mod / /; / = mod = / / | **Bob**; / Known / Unknown /; /---/---/; / = / /; / = / /; / = / /; / = mod / /; / = mod = / /; / = / /; / = mod / /; / = mod = / / | **Eve**; / Known / Unknown /; /---/---/; / = / /; / = / /; / /, /; / =, = / / |

Now is the shared secret key and it is known to both Alice and Bob, but *not* to Eve. Note that it is not helpful for Eve to compute, which equals ⁺ mod.

Note: It should be difficult for Alice to solve for Bob's private key or for Bob to solve for Alice's private key. If it is not difficult for Alice to solve for Bob's private key (or vice versa), then an eavesdropper, Eve, may simply substitute her own private / public key pair, plug Bob's public key into her private key, produce a fake shared secret key, and solve for Bob's private key (and use that to solve for the shared secret key). Eve may attempt to choose a public / private key pair that will make it easy for her to solve for Bob's private key.

### Generalization to finite cyclic groups

Here is a more general description of the protocol:

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Description, Ephemeral and/or static keys, Operation with more than two parties, Security and practical considerations, Other uses.)_
