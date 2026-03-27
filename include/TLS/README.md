# TLS

This folder contains simple scripts from various developers for low level encription, https and tls stuff. They are separate libraries but used together to form the basis of how KalaServer works with encryption and web safety.

Credits:
- [x25519](https://github.com/DavyLandman/compact25519)
- [hmac + sha256 aka hkdf](https://github.com/h5p9sl/hmac_sha256)
- [chacha20](https://github.com/Ginurx/chacha20-c)
- [poly1305](https://github.com/floodyberry/poly1305-donna)

order:
1) key agreement - x25519
2) key derivation - hkdf (hmac + sha256)
3) identity check - certificate from `/etc/ssl/certs/ca-certificates.crt`
4) secure transport - chacha20 + poly1305
