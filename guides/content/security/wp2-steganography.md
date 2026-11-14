---
id: security/wp2-steganography
title: Steganography
category: security
tags: [steganography, security]
summary: Steganography is the practice of representing information within another message or physical object, in such a manner that the presence of the concealed information would not be evident to an unsuspecting person's examination.
source: https://en.wikipedia.org/w/index.php?title=Steganography&oldid=1380713873
source_title: Wikipedia: Steganography
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Steganography", Wikipedia, The Free Encyclopedia (revision 1380713873), https://en.wikipedia.org/w/index.php?title=Steganography&oldid=1380713873 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Steganography

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Steganography** is the practice of representing information within another message or physical object, in such a manner that the presence of the concealed information would not be evident to an unsuspecting person's examination. In computing and electronic contexts, a computer file, message, image, or video is concealed within another file, message, image, or video. Generally, the hidden messages appear to be (or to be part of) something else: images, articles, shopping lists, or some other cover text. For example, the hidden message may be in invisible ink between the visible lines of a private letter. Some implementations of steganography that lack a formal shared secret are forms of security through obscurity, while key-dependent steganographic schemes try to adhere to Kerckhoffs's principle.

The word *steganography* comes from Greek *steganographia*, which combines the words *steganós* (el), meaning "covered or concealed", and *-graphia* (el) meaning "writing". The first recorded use of the term was in 1499 by Johannes Trithemius in his *Steganographia*, a treatise on cryptography and steganography, disguised as a book on magic.

The advantage of steganography over cryptography alone is that the intended secret message does not attract attention to itself as an object of scrutiny. Plainly visible encrypted messages, no matter how unbreakable they are, arouse interest and may in themselves be incriminating in countries in which encryption is illegal. Whereas cryptography is the practice of protecting the contents of a message alone, steganography is concerned with concealing both the fact that a secret message is being sent and its contents.

Steganography includes the concealment of information within computer files. In digital steganography, electronic communications may include steganographic coding inside a transport layer, such as a document file, image file, program, or protocol. Media files are ideal for steganographic transmission because of their large size. For example, a sender might start with an innocuous image file and adjust the color of every hundredth pixel to correspond to a letter in the alphabet. The change is so subtle that someone who is not looking for it is unlikely to notice the change.

## Techniques

Numerous techniques throughout history have been developed to embed a message within another medium.

### Physical

Placing the message in a physical item has been widely used for centuries. Some notable examples include invisible ink on paper, writing a message in Morse code on yarn worn by a courier, microdots, or using a music cipher to hide messages as musical notes in sheet music.

#### Social steganography

In communities with social or government taboos or censorship, people use cultural steganography—hiding messages in idiom, pop culture references, and other messages they share publicly and assume are monitored. This relies on social context to make the underlying messages visible only to certain readers. Examples include:

- Hiding a message in the title and context of a shared video or image.
- Misspelling names or words that are popular in the media in a given week, to suggest an alternative meaning.
- Hiding a picture that can be traced by using Paint or any other drawing tool.

### Digital messages

Since the dawn of computers, techniques have been developed to embed messages in digital cover mediums. The message to conceal is often encrypted, then used to overwrite part of a much larger block of encrypted data or a block of random data (an unbreakable cipher like the one-time pad generates ciphertexts that look perfectly random without the private key).

Examples of this include changing pixels in image or sound files, properties of digital text such as spacing and font choice, chaffing and winnowing, mimic functions, modifying the echo of a sound file (echo steganography), and including data in ignored sections of a file.

### Steganography in streaming media

Since the era of evolving network applications, steganography research has shifted from image steganography to steganography in streaming media such as Voice over Internet Protocol (VoIP).

In 2003, Giannoula et al. developed a data hiding technique leading to compressed forms of source video signals on a frame-by-frame basis.

In 2005, Dittmann et al. studied steganography and watermarking of multimedia contents such as VoIP.

In 2008, Yongfeng Huang and Shanyu Tang presented a novel approach to information hiding in low bit-rate VoIP speech stream, and their published work on steganography is the first-ever effort to improve the codebook partition by using Graph theory along with Quantization Index Modulation in low bit-rate streaming media.

In 2011 and 2012, Yongfeng Huang and Shanyu Tang devised new steganographic algorithms that use codec parameters as cover object to realise real-time covert VoIP steganography. Their findings were published in *IEEE Transactions on Information Forensics and Security*.

In 2024, Cheddad & Cheddad proposed a new framework for reconstructing lost or corrupted audio signals using a combination of machine learning techniques and latent information. The main idea of their paper is to enhance audio signal reconstruction by fusing steganography, halftoning (dithering), and state-of-the-art shallow and deep learning methods (e.g., RF, LSTM). This combination of steganography, halftoning, and machine learning for audio signal reconstruction may inspire further research in optimizing this approach or applying it to other domains, such as image reconstruction (i.e., inpainting).

### Adaptive steganography

Adaptive steganography is a technique for concealing information within digital media by tailoring the embedding process to the specific features of the cover medium. An example of this approach is demonstrated in the work. Their method develops a skin tone detection algorithm, capable of identifying facial features, which is then applied to adaptive steganography. By incorporating face rotation into their approach, the technique aims to enhance its adaptivity to conceal information in a manner that is both less detectable and more robust across various facial orientations within images. This strategy can potentially improve the efficacy of information hiding in both static images and video content.

### Cyber-physical systems/Internet of Things

Academic work since 2012 demonstrated the feasibility of steganography for cyber-physical systems (CPS)/the Internet of Things (IoT). Some techniques of CPS/IoT steganography overlap with network steganography, i.e. hiding data in communication protocols used in CPS/the IoT. However, specific techniques hide data in CPS components. For instance, data can be stored in unused registers of IoT/CPS components and in the states of IoT/CPS actuators.

### Printed

Digital steganography output may be in the form of printed documents. A message, the *plaintext*, may be first encrypted by traditional means, producing a *ciphertext*. Then, an innocuous *cover text* is modified in some way so as to contain the ciphertext, resulting in the *stegotext*. For example, the letter size, spacing, typeface, or other characteristics of a cover text can be manipulated to carry the hidden message. Only a recipient who knows the technique used can recover the message and then decrypt it. Francis Bacon developed Bacon's cipher as such a technique.

The ciphertext produced by most digital steganography methods, however, is not printable. Traditional digital methods rely on perturbing noise in the channel file to hide the message, and as such, the channel file must be transmitted to the recipient with no additional noise from the transmission. Printing introduces much noise in the ciphertext, generally rendering the message unrecoverable. There are techniques that address this limitation, one notable example being ASCII Art Steganography.

Although not classic steganography, some types of modern color laser printers integrate the model, serial number, and timestamps on each printout for traceability reasons using a dot-matrix code made of small, yellow dots not recognizable to the naked eye.

### Network

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Techniques, Additional terminology, Countermeasures and detection, Applications, Steganalysis.)_
