# Server-Client-Peer Network
This repository stores the code for my implementation of a project for my **Advanced Operating Systems** course at The Univeristy of Texas at Dallas. It is implemented to run exclusively on Linux machines with **C++11**.

## Requirements

### Server
* Serve any number of clients
* Respond to `Enquiry` messages with a list of managed files.
* Respond to `Read(file)` messages with the last line of a given file.
* Respond to `Append(file, line)` messages with `Ok` after appending the given line to the end of the given file.

### Client
* Connect to all other clients over a peer-to-peer network.
* Send `Enquiry` to one server for file names, which is then cached.
* Randomly generate `Read(file)` and `Write(line, (id, timestamp))` requests.
* For any request, gain mutual exclusion for the file across all clients.
* Send `Read` requests to a single server.
* Send `Write` requests to all servers.
* Implement the Ricart-Agrawala distributed mutual exclusion algorithm with the optimization proposed by Roucairol and Carvalho.

## Features
Some notable features of this implementation include:
* `program::options` - configurable command-line options parsing
* `program::properties` -- dynamic parsing of `.properties` files
* `util::buffer` - circular buffer type
* `util::optional` - C++11 implementation of an optional type
* `util::path` - C++11 implementation of a filesystem path
* `util::result` - C++11 implementation of a Rust-like result type
* `util::state_machine` - state machine with singleton states
