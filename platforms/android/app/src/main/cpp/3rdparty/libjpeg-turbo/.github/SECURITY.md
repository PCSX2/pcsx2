# Security Policy

## Supported Versions

Fixes for security vulnerabilities are proactively applied to any applicable
branch/release series that is in the
[Next-Gen, Active, Maintenance, or Extended support category](https://libjpeg-turbo.org/DeveloperInfo/Versioning).

## What Is a Security Vulnerability?

A security vulnerability is a bug in an official, supported release of
libjpeg-turbo whereby an otherwise well-behaved calling program can trigger a
potentially exploitable failure (such as a buffer overrun, an uninitialized
read, or undefined behavior) in one of the libraries by passing malformed image
data to a public API function.

- If the calling program itself is malformed and could not work properly with
  any image, then its inevitable failure is not a security vulnerability.  Such
  issues should be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md),
  and they will be investigated as opportunities for API hardening.

- If the issue affects only
  [Alpha/Evolving code](https://libjpeg-turbo.org/DeveloperInfo/Versioning) or
  has otherwise not officially been released, then it is not (yet) a security
  vulnerability.  Such issues should be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md).

- If the issue affects only an
  [EOL](https://libjpeg-turbo.org/DeveloperInfo/Versioning) branch/release
  series, then it is not a security vulnerability.  (Per above, fixes for
  security vulnerabilities are not proactively applied to EOL branches/release
  series.)  Such issues can be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md),
  but the suggested remedy will likely be to upgrade to a supported release.

## Reporting a Security Vulnerability

Vulnerabilities can be reported in one of the following ways:

- [E-mail the project admin](https://libjpeg-turbo.org/About/Contact).  You can
  optionally encrypt the e-mail using the provided public GPG key.

- [Beta and Post-Beta code](https://libjpeg-turbo.org/DeveloperInfo/Versioning)
  is not expected to be free of bugs, so vulnerabilities that affect only
  that code (for example, vulnerabilities introduced by a new feature that is
  not present in a Stable release series) can optionally be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md).
