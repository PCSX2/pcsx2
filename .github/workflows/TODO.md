# Improvements:

- I believe a ton of time (1minute on each) can be shaved off the linux builds by caching the apt dependencies - https://stackoverflow.com/a/59277514
  - however, there are a lot so, need to make some scripts to make this not a huge pain
- As the above is done, the linux build script can probably be refactored a bit, theres a bit of disabled paths, some naming, nothing major.
- Release to AUR on a tag
- Are we ready to do an osx build?
