Versioning
==========

Since version 3.0.0, VIXL uses [Semantic Versioning 2.0.0][semver].

Briefly:

- Backwards-incompatible changes update the _major_ version.
- New features update the _minor_ version.
- Bug fixes update the _patch_ version.

Why 3.0.0?
----------

VIXL was originally released as 1.x using snapshot releases. When we moved VIXL
into Linaro, we started working directly on `master` and stopped tagging
named releases. However, we informally called this "VIXL 2", so we are skipping
2.0.0 to avoid potential confusion.

Using `master`
--------------

Users who want to take the latest development version of VIXL can still take
commits from `master`. Our day-to-day development process hasn't changed and
these commits should still pass their own tests. However, note that commits not
explicitly tagged with a given version should be considered to be unversioned,
with no backwards-compatibility guarantees.

[semver]: https://semver.org/spec/v2.0.0.html
          "Semantic Versioning 2.0.0 Specification"
