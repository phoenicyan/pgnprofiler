
```
> test.exe -h
Usage:
    Test.exe [-p <path>] [-w] [-h]
Where:
    -p, --path
      Path to folder containing trace messages. Example: -p D:\data\*.pgl.
      Only files with .pgl or .csv extension can be processed.

    -w, --wait
      Wait for PGNProfiler to start before sending messages.

    -h, --help
      Displays usage information and exits.

    Stress test tool for PGNProfiler. Copyright (c) 2024 Intellisoft LLC

> test.exe -p miscdata\*.pgl
## Processing file trace1.pgl  numMessages=1884 
...
## Processing file treace_20201216_144915.pgl numMessages=16
Files: 117  Messages: 719302  Elapsed: 2.70873 
```
