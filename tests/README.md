## How to add unit tests
1. Add a new file named `*_test.cpp` directly into this folder.
2. Import individual includes you want to test, as isolate as possible from the hardware.
3. Write the test using the Unity framework (included as submodule).
4. From repository root folder, run `make test` to compile and run all tests.

### Notes
Unit tests are run in isolation on native platform. _It is not possible to include `Arduino.h` on native platform. All tested modules must be independent from it and mock the necessary functions_.