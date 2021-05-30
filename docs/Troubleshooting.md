# Troubleshooting
- Try to enable more verbose logs in Wuild config, adding "logLevel=6" in top of ini-file. logLevel=7 may be too verbose.
- Try to call Ninja wuild frontend with -v -n arguments (verbose dry run). You should see preprocess and compile invocations separately.
- Try to run different tests, e.g ```TestAllConfigs -c test.cpp -o test.o ``` - it emulates gcc commandline interface, but distributes build.
- Try to run WuildToolServerStatus and WuildCoordinatorStatus tools.
- Try to run manually WuildToolServer from the terminal, with full logging, and see what in the output during the build. (Or setup file logging)
