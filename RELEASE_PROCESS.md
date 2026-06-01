# Release process
A new version is released following these steps:
1. Update [library.properties](https://github.com/decadenza/ProsecCoAP/blob/main/library.properties) as required, using a new `vX.Y.Z` tag.
2. Create the corresponding `vX.Y.Z` tag and a new release (for GitHub and Arduino library manager). The logs for the Arduino library manager bot can be checked [here](https://downloads.arduino.cc/libraries/logs/github.com/decadenza/ProsecCoAP/).
3. Publish the new library to PlatformIO, running the following from the main project folder:
```
pio pkg publish .
```