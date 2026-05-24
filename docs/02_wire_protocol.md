# Wire Protocol

## Overview
The remote sync protocol is a framed TCP protocol used by client and server components.
It is designed around explicit message boundaries so partial reads and writes can be handled safely.

## Framing
Each frame should be encoded as:
```text
4-byte big-endian length
1-byte message type
payload bytes
```

The length field covers the message type byte and the payload.

## Message Types
- `HELLO`: client and server capability exchange.
- `MANIFEST`: file and chunk manifest for comparison.
- `HAVE_CHUNKS`: server response listing already stored chunks.
- `PUT_CHUNK`: upload of a missing chunk.
- `SNAPSHOT`: snapshot metadata exchange.
- `VERIFY`: request to verify stored data.
- `ERROR`: structured failure response.
- `OK`: success acknowledgement.

## Recommended Flow
1. Client opens TCP connection.
2. Client sends `HELLO`.
3. Client sends a `MANIFEST` for the source tree.
4. Server responds with `HAVE_CHUNKS` or missing-item details.
5. Client uploads missing chunks with `PUT_CHUNK`.
6. Server persists data and returns `OK` or `ERROR`.

## Payload Guidance
- Use UTF-8 text for human-readable fields.
- Use fixed-width integers for lengths and counters.
- Use lowercase hex for chunk hashes.
- Keep payloads self-describing where possible.

## Error Handling
- Reject frames that are too large.
- Reject malformed lengths.
- Reject unknown message types unless a capability handshake allows extension.
- Abort on protocol desynchronization instead of guessing.

## Security Notes
- The protocol should be safe against path traversal in remote file references.
- All sensitive data should still be protected by the encryption layer.
- Authentication and stronger transport protection can be added later, but framing must remain strict now.
