# SkyX - Simple P2P File Sharing Application

SkyX is a personal project of mine to familiarize myself with C++ and its ecosystem of tools and libraries. The project is built on Mac OS but should work on every platform.

**NOTE: THIS IS VERY MUCH WORK IN PROGRESS AND THE FOLLOWING REPRESENT A FUTURE STATE OF THE APPLICATION**

## Architecture
A SkyX network is comprised of two components:
1. Tracker - Used for peer discovery.
2. Client (Peer) - A user node on the system.

### Tracker Registration Flow & Peer Discovery
Before connecting to other peers, we need to register to a known tracker. The flow is as follows:
1. Establish a TCP connection between the tracker and our local client node.
2. On successful establishment, we try to perform a UDP hole punch to open a port (UPnP registration TBA).
3. We ask the tracker for a list of available peers. We can filter peers by given attributes, ex: `client version == 0.1a`.
4. Peers don't really need to know each other, just the fact that they can send UDP packets to and from one another. In other words **if we register our local node to a tracker we accept the condition that other peers can automatically connect to us** (I plan to make this configurable tho).
5. After receiving the peers list, the local client asks them to share the exposed files they have indexed.

**NOTE**: Peers cannot share information about their connected peers. Therefore, message broadcasting is limited to a single hop, from the source peer down to every peer that is connected to it, but not peers-of-peers.

### File Sharing
A local node only exposes files that have been explicitly indexed. To index a file you would need to run:
```
skyx index add /path/to/file.<extension>
```
or
```
skyx index add -d /folder/to/recursively/index/
```

You can validate the index if you need to before starting the local node:
```
skyx index validate
```

Alternatively, the client will validate each indexed file before starting to seed them.
A client can request a known file from one or more of its peers, and then start receiving that file from all the peers that have it. Files are being sent in chunks, possibly sequentially (although this won't be enforced).
Once a new peer has a file, they can contribute to sharing it from the other peers. To notify the swarm of a new available seeder, we broadcast a message to every known peer.

Libraries used to build an index:
- sqlite3 - Local DB to store the whole thing
- BLAKE3 - Hash algorithm to compute file hashing
- OpenSSL - Used to compute SHA-256 hashes

### Application startup
A full set of commands to bootstrap a local node:
```
skyx init
skyx trackers add XXXX.XXXX.XXXX.XXXX:8080
skyx index add -d data
skyx start
```

After that you can restart it by simply running
```
skyx index validate && skyx start
```

You can view a list of all the files exposed by the connected peers via:
```
skyx index list --remote   # removing --remote will display the local indexed files :)
```

To automatically download a known file:
```
skyx fetch <file path key hash>
```
where `<file path key hash>` is the `path_key` of the specific indexed file in the database. This will save the file on the current working directory.

SkyX allows you to access a remote peer's file(s) similar to what you'd do on a network storage if you know the file path:
```
skyx fetch /this/path/is/on/another/machine/cool_song.mp3
```
