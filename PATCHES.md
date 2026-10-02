# Fork patches over upstream UAPKI

Changes to the upstream library code (`library/uapki`, `library/uapkic`, `library/uapkif`) that must be
re-applied when merging a new upstream release. Every hunk is marked with `UAPKI-1C PATCH <name>`.
The 1C AddIn itself (`library/uapki-1c`) is fork-only and is not listed here.

## ocsp-responder-same-provider

Files: `library/uapki/src/cert-validator.cpp`, `library/uapki/src/cert-validator.h`
(`CertValidator::acceptResponderOfSameProvider`, the `ProviderId` helper, one extra condition in
`CertValidator::getStatus`).

**Problem.** In 2026 ЦСК «Україна» re-keyed its CA: UA-36865753-2401 (under ЦЗО root UA-43220851-2020) was
followed by UA-36865753-2602 (under ЦЗО root UA-43220851-2601). Its OCSP service answers for certificates of
the old CA 2401 with the responder UA-36865753-2604, which is issued by the new CA 2602. Upstream
`getStatus` (used by SIGN, BUILD_CMS 2-pass and the TSP-certificate check) requires the responder's
AuthorityKeyId or KeyId to belong to the subject's own chain, so SIGN fails with
`CERT_CHAIN_NOT_FOUND` (4168) and `expectedCerts = [{entity: "OCSP", <the responder itself>}]`.
ІІТ and ЦЗО/Дія accept such responses.

**Change.** When the upstream chain-membership check fails, the responder is still accepted if all of
the following hold:

1. it has EKU id-kp-OCSPSigning;
2. its own chain builds from the certificate store to a self-signed CA root. Each signature verifies with
   status VALID, every issuer is a CA, and every cert is valid at the validation time;
3. that root is **trusted** (INIT `certCache.trustedCerts`) or is the root of the subject's own chain;
4. the responder's issuer and the issuer of every subject it answered for are the same provider (КНЕДП):
   - if both have `organizationIdentifier` (2.5.4.97), it must be equal (`NTRUA-<EDRPOU>`);
   - otherwise (АЦСК-era certs) the EDRPOU must match, taken from organizationIdentifier or from
     `serialNumber` `UA-<EDRPOU>-<N>`, and `O` must match exactly.

Otherwise the upstream behaviour (4168 + expectedCerts) is kept. Accepted responders add their chain
(CA + root) to the obtained certs, so CAdES-C/XL certificate-refs/-values carry it.

**Why the trust requirement.** SIGN never checks trust anchors. Certificates embedded in an OCSP
response are added to the store before the responder is looked up, so an unanchored, name-only rule
would let a forged plain-HTTP OCSP response bring its own CA chain with a matching organizationIdentifier.
Requirement 3 doubles as the switch: with an empty `trustedCerts` and a responder under a different
root, nothing changes compared to upstream. To enable it for ЦСК «Україна», pass the ЦЗО root
UA-43220851-2601 (SKI `D7:2B:26:CB:…`) in INIT `certCache.trustedCerts`.

**Not covered.** The revocation status of the responder's issuer (CA 2602) is not checked, the same as
upstream for any responder's issuer. VERIFY is unaffected (`doc-verify.cpp` only checks the OCSP
response signature). The CRL branch of `getStatus` has an analogous membership check for the CRL issuer.
It is not relaxed.
