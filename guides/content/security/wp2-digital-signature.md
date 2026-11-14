---
id: security/wp2-digital-signature
title: Digital signature
category: security
tags: [digital signature, security]
summary: A digital signature is a mathematical scheme for verifying the authenticity of digital messages or documents.
source: https://en.wikipedia.org/w/index.php?title=Digital_signature&oldid=1383505278
source_title: Wikipedia: Digital signature
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Digital signature", Wikipedia, The Free Encyclopedia (revision 1383505278), https://en.wikipedia.org/w/index.php?title=Digital_signature&oldid=1383505278 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Digital signature

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **digital signature** is a mathematical scheme for verifying the authenticity of digital messages or documents. A valid digital signature on a message gives any recipient confidence that the message came from a sender known to the recipient. In contrast, a message authentication code only gives confidence to a specific recipient that the message came from a known sender.

Digital signatures are a type of public-key cryptography, and are commonly used for software distribution,
financial transactions, contract management software, and in other cases where it is important to detect forgery or tampering.

A digital signature on a message or document is similar to a handwritten signature on paper, but it is not restricted to a physical medium like paper—any bitstring can be digitally signed—and while a handwritten signature on paper could be copied onto other paper in a forgery, a digital signature on a message is mathematically bound to the *content* of the message so that it is infeasible for anyone to forge a valid digital signature on any other message.

Digital signatures are often used to implement electronic signatures, which include any electronic data that carries the intent of a signature, but not all electronic signatures use digital signatures.

## Definition

A digital signature scheme consists of three algorithms:

- A **key generation** algorithm that selects a **private key** at random from a set of possible private keys. The algorithm outputs the private key and a corresponding **public key**.
- A **signing** algorithm that, given a message and a private key, produces a signature.
- A **signature verifying** algorithm that, given the message, public key and signature, either accepts or rejects the message's claim to authenticity.

Two main properties are required:

1. *Correctness*: Signatures produced by the signing algorithm with a private key pass the verification algorithm with the corresponding public key.
1. *Security* (existential unforgeability under chosen-message attack, or EUF-CMA): It should be computationally infeasible to generate a valid signature for a party without knowing that party's private key.

Formally, a digital signature scheme is a triple of probabilistic polynomial-time algorithms, (*G*, *S*, *V*), satisfying:

- *G* (key-generator) generates a public key (*pk*), and a corresponding private key (*sk*), on input 1^*n*, where *n* is the security parameter.
- *S* (signing) returns a tag, *t*, on the inputs: the private key (*sk*), and a string (*x*).
- *V* (verifying) outputs *accepted* or *rejected* on the inputs: the public key (*pk*), a string (*x*), and a tag (*t*).

Here 1^*n* refers to a unary number in the formalism of computational complexity theory.

For *correctness*, *S* and *V* must satisfy

Pr [(*pk*, *sk*) ← *G*(1^*n*), *V*(*pk*, *x*, *S*(*sk*, *x*)) = *accepted*] = 1.

A digital signature scheme is *secure* if for every non-uniform probabilistic polynomial time adversary *A*,

Pr [(*pk*, *sk*) ← *G*(1^*n*), (*x*, *t*) ← *A*^*S*(*sk*, · )(*pk*, 1^*n*), *x* ∉ *Q*, *V*(*pk*, *x*, *t*) = *accepted*] < negl(*n*),

where *A*^*S*(*sk*, · ) denotes that *A* has access to the oracle, *S*(*sk*, · ), *Q* denotes the set of the queries on *S* made by *A*, which knows the public key, *pk*, and the security parameter, *n*, and *x* ∉ *Q* denotes that the adversary may not directly query the string, *x*, on *S*.

## Method

One digital signature scheme (of many) is based on RSA. To create signature keys, generate an RSA key pair containing a modulus, *N*, that is the product of two random secret distinct large primes, along with integers, *e* and *d*, such that *e* *d* ≡ 1 (mod *φ*(*N*)), where *φ* is Euler's totient function. The signer's public key consists of *N* and *e*, and the signer's secret key contains *d*.

Used directly, this type of signature scheme is vulnerable to key-only existential forgery attack. To create a forgery, the attacker picks a random signature σ and uses the verification procedure to determine the message, *m*, corresponding to that signature. In practice, however, this type of signature is not used directly, but rather, the message to be signed is first hashed to produce a short digest, that is then padded to larger width comparable to *N*, then signed with the reverse trapdoor function. This forgery attack, then, only produces the padded hash function output that corresponds to σ, but not a message that leads to that value, which does not lead to an attack. In the random oracle model, hash-then-sign (an idealized version of that practice where hash and padding combined have close to *N* possible outputs), this form of signature is existentially unforgeable, even against a chosen-plaintext attack.

There are several reasons to sign such a hash (or message digest) instead of the whole document.

**For efficiency The signature will be much shorter and thus save time since hashing is generally much faster than signing in practice.For compatibility Messages are typically bit strings, but some signature schemes operate on other domains (such as, in the case of RSA, numbers modulo a composite number *N*). A hash function can be used to convert an arbitrary input into the proper format.For integrity Without the hash function, the text "to be signed" may have to be split (separated) in blocks small enough for the signature scheme to act on them directly. However, the receiver of the signed blocks is not able to recognize if all the blocks are present and in the appropriate order.**

## Notions of security

In their foundational paper, Goldwasser, Micali, and Rivest lay out a hierarchy of attack models against digital signatures:

1. In a *key-only* attack, the attacker is only given the public verification key.
1. In a *known message* attack, the attacker is given valid signatures for a variety of messages known by the attacker but not chosen by the attacker.
1. In an *adaptive chosen message* attack, the attacker first learns signatures on arbitrary messages of the attacker's choice.

They also describe a hierarchy of attack results:

1. A *total break* results in the recovery of the signing key.
1. A universal forgery attack results in the ability to forge signatures for any message.
1. A selective forgery attack results in a signature on a message of the adversary's choice.
1. An existential forgery merely results in some valid message/signature pair not already known to the adversary.

The strongest notion of security, therefore, is security against existential forgery under an adaptive chosen message attack.

## Some digital signature algorithms

- RSA
- DSA
- ECDSA
- EdDSA
- RSA with SHA
- ECDSA with SHA
- ElGamal signature scheme as the predecessor to DSA, and variants Schnorr signature and Pointcheval–Stern signature algorithm
- Rabin signature algorithm
- Pairing-based schemes such as BLS
- CRYSTALS-Dilithium, a quantum-resistant scheme based on LWE in lattices
- Falcon, a quantum-resistant scheme based on CVP in lattices
- SPHINCS+, a quantum-resistant scheme based on hash functions
- Undeniable signatures
- Aggregate signature – a signature scheme that supports aggregation: Given n signatures on n messages from n users, it is possible to aggregate all these signatures into a single signature whose size is constant in the number of users. This single signature will convince the verifier that the n users did indeed sign the n original messages. A scheme by Mihir Bellare and Gregory Neven may be used with Bitcoin.
- Signatures with efficient protocols – are signature schemes that facilitate efficient cryptographic protocols such as zero-knowledge proofs or secure computation.

_(Later sections of the original article were left out of this offline copy for length: Applications, Additional security precautions, The current state of use – legal and practical, Industry standards.)_
