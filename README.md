# pbench

The pbench is Windows equivalent to Unix "time" command to measure
process resource usage.

## Installation

> [!NOTE]
> Command prompt window should be restarted after installation to
> find installed pbench.exe

Using Windows Installer GUI:
1. Download [latest release](https://github.com/chemodax/pbench/releases/latest)
2. Run the installation package

## Syntax

```
pbench [OPTIONS] COMMAND [ARGS]
```

## Parameters
| Parameter        | Description                                           |
|------------------|-------------------------------------------------------|
| --pid PID        | Also benchmark process with specified process ID      |
| --count COUNT    | Run command COUNT times                               |
| --concurrent     | Run command in parallel                               |
| --stdout-nul     | Redirect command output to NUL                        |

## Examples

Measure performance of `curl http://example.com` command:

```Batchfile
pbench --stdout-nul curl http://example.com
```

```Console
==== curl http://example.com ====

== Performance of curl http://example.com
   Real time:    0.299 s
    CPU time:    0.015 s
   User time:    0.000 s
 Kernel time:    0.015 s
     Read IO:        0 (       0 KB)
    Write IO:        1 (       1 KB)
    Other IO:      127 (       1 KB)
Peak WS size:     6196 KB

  Total time:    0.303 s
```
