# P4 Source

The P4 source for the four programs measured in this project is maintained in a
separate repository, not vendored into this repo.

**Repository:** https://github.com/OCT-FPGA/P4Framework
**Branch:** `pavani-P4Framework`
**Reference commit:** `0ccdef6`

| Program      | Path                          |
|--------------|-------------------------------|
| Forward      | `Examples/forward/forward.p4` |
| FiveTuple    | `Examples/fiveTuple/`         |
| Checksum     | `Examples/checksum/`          |
| RemoveHeader | `Examples/rmvhdr/`            |

The compiled bitstreams in `../bitstreams/` were built from this source at the
reference commit above. To rebuild, check out that commit and follow the build
steps in each program's directory.
