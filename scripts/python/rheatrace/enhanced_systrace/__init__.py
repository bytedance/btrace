#!/usr/bin/env python
import os
import sys


def get_rhea_trace_dir():
    return os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))


def _add_dir_to_python_path(*path_parts):
    path = os.path.abspath(os.path.join(*path_parts))
    if os.path.isdir(path) and path not in sys.path:
        # Some callsite that use telemetry assumes that sys.path[0] is the directory
        # containing the script, so we add these extra paths to right after it.
        sys.path.insert(1, path)


_add_dir_to_python_path(get_rhea_trace_dir())
