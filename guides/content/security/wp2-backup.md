---
id: security/wp2-backup
title: Backup
category: security
tags: [backup, data backup, restore, security]
summary: In information technology, a backup, or data backup is a copy of computer data taken and stored elsewhere so that it may be used to restore the original after a data loss event.
source: https://en.wikipedia.org/w/index.php?title=Backup&oldid=1383452388
source_title: Wikipedia: Backup
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Backup", Wikipedia, The Free Encyclopedia (revision 1383452388), https://en.wikipedia.org/w/index.php?title=Backup&oldid=1383452388 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Backup

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In information technology, a **backup**, or **data backup** is a copy of computer data taken and stored elsewhere so that it may be used to **restore** the original after a data loss event. The verb form, referring to the process of doing so, is "back up", whereas the noun and adjective form is "backup". Backups can be used to recover data after its loss from data deletion or corruption, or to recover data from an earlier time. Backups provide a simple form of IT disaster recovery; however not all backup systems are able to reconstitute a computer system or other complex configuration such as a computer cluster, active directory server, or database server.

A backup system contains at least one copy of all data considered worth saving. The data storage requirements can be large. An information repository model may be used to provide structure to this storage. There are different types of data storage devices used for copying backups of data that is already in secondary storage onto archive files. There are also different ways these devices can be arranged to provide geographic dispersion, data security, and portability.

Data is selected, extracted, and manipulated for storage. The process can include methods for dealing with live data, including open files, as well as compression, encryption, and de-duplication. Additional techniques apply to enterprise client-server backup. Backup schemes may include dry runs that validate the reliability of the data being backed up. There are limitations and human factors involved in any backup scheme.

## Storage

A backup strategy requires an information repository, "a secondary storage space for data" that aggregates backups of data "sources". The repository could be as simple as a list of all backup media (DVDs, etc.) and the dates produced, or could include a computerized index, catalog, or relational database.

### 3-2-1 backup rule

The backup data needs to be stored, requiring a backup rotation scheme, which is a system of backing up data to computer media that limits the number of backups of different dates retained separately, by appropriate re-use of the data storage media by overwriting of backups no longer needed. The scheme determines how and when each piece of removable storage is used for a backup operation and how long it is retained once it has backup data stored on it. The 3-2-1 rule can aid in the backup process. It states that there should be at least 3 copies of the data, stored on 2 different types of storage media, and one copy should be kept offsite, in a remote location (this can include cloud storage). 2 or more different media should be used to eliminate data loss due to similar reasons (for example, optical discs may tolerate being underwater while LTO tapes may not, and SSDs cannot fail due to head crashes or damaged spindle motors since they do not have any moving parts, unlike hard drives). An offsite copy protects against fire, theft of physical media (such as tapes or discs) and natural disasters like floods and earthquakes. Physically protected hard drives are an alternative to an offsite copy, but they have limitations like only being able to resist fire for a limited period of time, so an offsite copy still remains as the ideal choice.

Because there is no perfect storage, many backup experts recommend maintaining a second copy on a local physical device, even if the data is also backed up offsite. Ransomware can still overwrite data on a backup drive if it is plugged and accessible to a computer. Because of this, it is recommended to disconnect at least one of the backup media if not in use.

### Backup methods

#### Unstructured

An unstructured repository may simply be a stack of tapes, DVD-Rs or external HDDs with minimal information about what was backed up and when. This method is the easiest to implement, but unlikely to achieve a high level of recoverability as it lacks automation.

#### Full only/System imaging

A repository using this backup method contains complete source data copies taken at one or more specific points in time. Copying system images, this method is frequently used by computer technicians to record known good configurations. However, imaging is generally more useful as a way of deploying a standard configuration to many systems rather than as a tool for making ongoing backups of diverse systems.

#### Incremental

An incremental backup stores data changed since a reference point in time. Duplicate copies of unchanged data are not copied. Typically a full backup of all files is made once or at infrequent intervals, serving as the reference point for an incremental repository. Subsequently, a number of incremental backups are made after successive time periods. Restores begin with the last full backup and then apply the incrementals.
Some backup systems can create a synthetic full backup from a series of incrementals, thus providing the equivalent of frequently doing a full backup. When done to modify a single archive file, this speeds restores of recent versions of files.

#### Near-CDP

Continuous Data Protection (CDP) refers to a backup that instantly saves a copy of every change made to the data. This allows restoration of data to any point in time and is the most comprehensive and advanced data protection. Near-CDP backup applications—often marketed as "CDP"—automatically take incremental backups at a specific interval, for example every 15 minutes, one hour, or 24 hours. They can therefore only allow restores to an interval boundary. Near-CDP backup applications use journaling and are typically based on periodic "snapshots", read-only copies of the data frozen at a particular point in time.

Near-CDP (except for Apple Time Machine) intent-logs every change on the host system, often by saving byte or block-level differences rather than file-level differences. This backup method differs from simple disk mirroring in that it enables a roll-back of the log and thus a restoration of old images of data. Intent-logging allows precautions for the consistency of live data, protecting *self-consistent* files but requiring *applications* "be quiesced and made ready for backup."

Near-CDP is more practicable for ordinary personal backup applications, as opposed to *true* CDP, which must be run in conjunction with a virtual machine or equivalent and is therefore generally used in enterprise client-server backups.

Software may create copies of individual files such as written documents, multimedia projects, or user preferences, to prevent failed write events caused by power outages, operating system crashes, or exhausted disk space, from causing data loss. A common implementation is an appended ".bak" extension to the file name.

#### Reverse incremental

A Reverse incremental backup method stores a recent archive file "mirror" of the source data and a series of differences between the "mirror" in its current state and its previous states. A reverse incremental backup method starts with a non-image full backup. After the full backup is performed, the system periodically synchronizes the full backup with the live copy, while storing the data necessary to reconstruct older versions. This can either be done using hard links—as Apple Time Machine does, or using binary diffs.

#### Differential

A differential backup saves only the data that has changed since the last full backup. This means a maximum of two backups from the repository are used to restore the data. However, as time from the last full backup (and thus the accumulated changes in data) increases, so does the time to perform the differential backup. Restoring an entire system requires starting from the most recent full backup and then applying just the last differential backup.

A differential backup copies files that have been created or changed since the last full backup, regardless of whether any other differential backups have been made since, whereas an incremental backup copies files that have been created or changed since the most recent backup of any type (full or incremental). Changes in files may be detected through a more recent date/time of last modification file attribute, and/or changes in file size. Other variations of incremental backup include multi-level incrementals and block-level incrementals that compare parts of files instead of just entire files.

### Storage media

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Storage, Selection and extraction of data, Manipulation of data and dataset optimization.)_
