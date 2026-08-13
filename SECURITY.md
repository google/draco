## Security Model

Draco is a library for compressing and decompressing 3D geometric meshes and
point clouds. The library is designed to be fast and efficient, and it can be
used in a variety of applications, from real-time rendering to offline
processing.

The Draco decoder API in the `draco::Decoder` class and the `draco_decoder`
binary are designed to be robust against malicious or malformed input data.
These components can be safely used to decode untrusted input data from
untrusted sources.

All other Draco APIs and binaries, including but not limited to the encoder API,
`draco_encoder`, and `draco_transcoder`, are not hardened against malicious
input and should only be used on trusted input data. Using these components on
untrusted data may lead to security vulnerabilities such as crashes or memory
corruption.

## Security and Vulnerability Reporting

Please use https://g.co/vulnz to report security vulnerabilities.

We use https://g.co/vulnz for our intake and triage. For valid issues we will do
coordination and disclosure here on GitHub (including using a GitHub Security
Advisory when necessary).

The Google Security Team will process your report within a day, and respond
within a week (although it will depend on the severity of your report).
