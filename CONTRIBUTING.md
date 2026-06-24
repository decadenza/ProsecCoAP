# Contributing to ProsecCoAP
Contributions are welcome. You may contribute by:
- Opening issues to report bugs
- Ask for missing features.
- Make pull requests.

## Coding standards
Code must be documented using [Doxygen](https://www.doxygen.nl/manual/docblocks.html).

Please write code following the [Arduino Style Guide](https://docs.arduino.cc/learn/contributions/arduino-library-style-guide/) as much as possible.

## Unit tests
Unit tests for pure C++ functions can be added in the [tests](./tests) folder.
All the files matching the pattern `tests/*_test.cpp` will be run as tests as part of the integrated workflow.

To run tests on your machine, use the command:
```
make test
```
Requires `g++` installed.

**NOTE**: I have not found a straightforward way to test anything that uses `<Arduino.h>` on integrated workflows.
For testability, wherever possible, keep pure C++ files separate (see, for example, [Utils](src/Utils.h)).
Suggestions are welcome.