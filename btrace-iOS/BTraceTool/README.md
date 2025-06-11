btrace is a command line tool to record trace data and display flamegraph offline for iOS apps which are built with 'BTrace' and 'BTraceDebug'.

## Installation
```bash
# using homebrew
brew install libusbmuxd
brew install poetry
# install btrace from the top-level 'BTraceTool' directory
poetry install
```

## Usage
### Record

Note that if '-l' is not specified, app must have been launched before recording.

```bash
python3 -m btrace record [-h] [-i DEVICE_ID] [-b BUNDLE_ID] [-o OUTPUT] [-t TIME_LIMIT] [-d DSYM_PATH] [-m] [-l] [-s]
```
### Options
| Option | Description          |
|---------------------------------------| -----------  |
| -h, --help                | Show help      |
| -i DEVICE_ID, --device_id DEVICE_ID |  Device id. If not specified: <br>· If only one device is connected to Mac, it will be chosen <br>· If multiple devices are connected to Mac, prompt the user to make a selection |
| -b BUNDLE_ID    --bundle_id BUNDLE_ID |  Bundle id |
| -o OUTPUT       --output OUTPUT | Output path. If not specified, data will be saved to '~/Desktop/btrace'|
| -t TIME_LIMIT   --time_limit TIME_LIMIT | Limit recording time, default 3600s |
| -d DSYM_PATH    --dsym_path DSYM_PATH |  Dsym file path, or app path built by Xcode in debug mode. If specified，flamegraph will be displayed automatically after the recording ends |
| -m    --main_thread_only |  If given, only record main thread trace data |
| -l    --launch  |  If given, app will be launched/relaunched, and start recording on app launch|
| -s    --sys_symbol  |  If given, symbols in the system libraries will be parsed |
### Examples
```bash
python3 -m btrace record -i xxx -b xxx -d /xxxDebug-iphoneos/xxx.app
python3 -m btrace record -i xxx -b xxx -d /xxxDebug-iphoneos/xxx.dSYM
```

## Stop
```bash
ctrl + c
```

## Parse

When should the 'parse' command used?
- '-d' option is not specified in the 'record' command.
- reopen the parsed data

```bash
python3 -m btrace parse [-h] [-d DSYM_PATH] [-f] [-s] file_path
```
### Options
| Option                       | Description          |
|---------------------------------------| -----------  |
| -h, --help                | Show help      |
| -d DSYM_PATH, --dsym_path DSYM_PATH |  Dsym file path, or app path built by Xcode in debug mode |
| -s, --sys_symbol  |  If given, symbols in the system libraries will be parsed |
| -f, --force  |  If given, force re-parsing trace data |
### Examples
```bash
btrace parse -d /xxx.dSYM xxx.sqlite
btrace parse -d /xxx.app xxx.sqlite
```