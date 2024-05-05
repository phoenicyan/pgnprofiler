# pgnprofiler
Open source version of Intellisoft's PGNProfiler

## Purpose

Need an efficient tool to collect trace/profiling info from Intellisoft OLE DB Provider for PostgreSQL. The tool should be able to read very long traces from Windows applications and Services, change logging levels, filter for errors, store traces to files, and display previously stored files. It has advanced features like injecting itself into another computer where PGNP was not installed and collecting a remote trace. It supports light and dark themes. Etc.

## Compilation
The project compiles with Visual Studio 2022 Community Edition.

### Prerequisites

Boost 1.70 with chrono, date_time, interprocess, regex installed into C:\Program Files (x86). Boost should be compiled for static linking.


## History

I solely created the PGNProfiler in 2009. I got inspiration from another similar tool that I contributed a few years before. That other tool was written in Delphi 6/7 (Pascal). It provided me with good idea what is expected from a log viewer/tracer.

