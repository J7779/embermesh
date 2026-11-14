---
id: security/wp2-web-of-trust
title: Web of trust
category: security
tags: [web of trust, security]
summary: In cryptography, a web of trust is a concept used in PGP, GnuPG, and other OpenPGP-compatible systems to establish the authenticity of the binding between a public key and its owner.
source: https://en.wikipedia.org/w/index.php?title=Web_of_trust&oldid=1381233886
source_title: Wikipedia: Web of trust
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Web of trust", Wikipedia, The Free Encyclopedia (revision 1381233886), https://en.wikipedia.org/w/index.php?title=Web_of_trust&oldid=1381233886 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Web of trust

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In cryptography, a **web of trust** is a concept used in PGP, GnuPG, and other OpenPGP-compatible systems to establish the authenticity of the binding between a public key and its owner. Its decentralized trust model is an alternative to the centralized trust model of a public key infrastructure (PKI), which relies exclusively on a certificate authority (or a hierarchy of such). As with computer networks, there are many independent webs of trust, and any user (through their public key certificate) can be a part of, and a link between, multiple webs.

The web of trust concept was first put forth by PGP creator Phil Zimmermann in 1992 in the manual for PGP version 2.0:

> As time goes on, you will accumulate keys from other people that you may want to designate as trusted introducers. Everyone else will each choose their own trusted introducers. And everyone will gradually accumulate and distribute with their key a collection of certifying signatures from other people, with the expectation that anyone receiving it will trust at least one or two of the signatures. This will cause the emergence of a decentralized fault-tolerant web of confidence for all public keys.

Note the use of the word emergence in this context. The web of trust makes use of the concept of emergence.

## Operation of a web of trust

All OpenPGP-compliant implementations include a certificate vetting scheme to assist with this; its operation has been termed a web of trust. OpenPGP certificates (which include one or more public keys along with owner information) can be digitally signed by other users who, by that act, endorse the association of that public key with the person or entity listed in the certificate. This is commonly done at key signing parties.

OpenPGP-compliant implementations also include a vote counting scheme which can be used to determine which public key – owner association a user will trust while using PGP. For instance, if three partially trusted endorsers have vouched for a certificate (and so its included public key – owner binding), or if one fully trusted endorser has done so, the association between owner and public key in that certificate will be trusted to be correct. The parameters are user-adjustable (e.g., no partials at all, or perhaps six partials) and can be completely bypassed if desired.

The scheme is flexible, unlike most public key infrastructure designs, and leaves trust decisions in the hands of individual users. It is not perfect and requires both caution and intelligent supervision by users. Essentially all PKI designs are less flexible and require users to follow the trust endorsement of the PKI generated, certificate authority (CA)-signed, certificates.

### Simplified explanation

There are two keys pertaining to a person: a public key which is shared openly and a private key that is withheld by the owner. The owner's private key will decrypt any information encrypted with its public key. In the web of trust, each user has a key ring with other people's public keys.

Senders encrypt their information with the recipient's public key, and only the recipient's private key will decrypt it. Each sender then digitally signs the encrypted information with their private key. When the recipient verifies the received encrypted information against the sender's public key, they can confirm that it is from the sender. Doing this will ensure that the encrypted information came from the specific user and has not been tampered with, and only the intended recipient can decrypt the information (because only they know their private key).

## Contrast with typical PKI

Unlike WOT, a typical X.509 PKI enables each certificate to be signed by a single party: a certificate authority (CA). The CA's certificate may itself be signed by a different CA, all the way up to a 'self-signed' root certificate. Root certificates must be available to those who use a lower-level CA certificate and so are typically distributed widely. They are for instance, distributed with such applications as browsers and email clients. In this way SSL/TLS-protected Web pages, email messages, etc. can be authenticated without requiring users to manually install root certificates. Applications commonly include over one hundred root certificates from dozens of PKIs, thus by default bestowing trust throughout the hierarchy of certificates which lead back to them.

WOT favors the decentralization of trust anchors to prevent a single point of failure from compromising the CA hierarchy.

## Problems

### Loss of private keys

The OpenPGP web of trust is essentially unaffected by such things as company failures, and has continued to function with little change. However, a related problem does occur: users, whether individuals or organizations, who lose track of a private key can no longer decrypt messages sent to them produced using the matching public key found in an OpenPGP certificate. Early PGP certificates did not include expiry dates, and those certificates had unlimited lives. Users had to prepare a signed cancellation certificate against the time when the matching private key was lost or compromised. Prominent cryptographer Bruce Schneier is still getting messages encrypted using a public key for which he long ago lost track of the private key. They can't do much with those messages except discard them after notifying the sender that they were unreadable and requesting resending with a public key for which they still have the matching private key. Later PGP, and all OpenPGP compliant certificates include expiry dates which automatically preclude such troubles (eventually) when used sensibly. This problem can also be easily avoided by the use of "designated revokers", which were introduced in the early 1990s. A key owner may designate a third party that has permission to revoke the key owner's key (in case the key owner loses their own private key and thus loses the ability to revoke their own public key).

### Public key authenticity check

A non-technical, social difficulty with a Web of Trust like the one built into PGP/OpenPGP type systems is that every web of trust without a central controller (e.g., a CA) depends on other users for trust. Those with new certificates (i.e., produced in the process of generating a new key pair) will not likely be readily trusted by other users' systems, that is by those they have not personally met, until they find enough endorsements for the new certificate. This is because many other Web of Trust users will have their certificate vetting set to require one or more fully trusted endorsers of an otherwise unknown certificate (or perhaps several partial endorsers) before using the public key in that certificate to prepare messages, believe signatures, etc.

Despite the wide use of OpenPGP compliant systems and easy availability of on-line multiple key servers, it is possible in practice to be unable to readily find someone (or several people) to endorse a new certificate (e.g., by comparing physical identification to key owner information and then digitally signing the new certificate). Users in remote areas or undeveloped ones, for instance, may find other users scarce. And, if the other's certificate is also new (and with no or few endorsements from others), then its signature on any new certificate can offer only marginal benefit toward becoming trusted by still other parties' systems and so able to securely exchange messages with them. Key signing parties are a relatively popular mechanism to resolve this problem of finding other users who can install one's certificate in existing webs of trust by endorsing it. Websites also exist to facilitate the location of other OpenPGP users to arrange keysignings. The Gossamer Spider Web of Trust also makes key verification easier by linking OpenPGP users via a hierarchical style web of trust where end users can benefit by coincidental or determined trust of someone who is endorsed as an introducer, or by explicitly trusting GSWoT's top-level key minimally as a level 2 introducer (the top-level key endorses level 1 introducers).

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Problems, Strong set, Mean shortest distance, WOT assisting solutions.)_
