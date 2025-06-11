#!/usr/bin/env python3
# Copyright (C) 2025 ByteDance Inc.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Includes work from The Android Open Source Project (https://android.googlesource.com/)
# with modifications.

# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import http.server
import socketserver
import webbrowser


# HTTP Server used to open the trace in the browser.
class HttpHandler(http.server.SimpleHTTPRequestHandler):

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", self.server.allow_origin)
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_GET(self):
        if self.path != "/" + self.server.expected_fname:
            self.send_error(404, "File not found")
            return

        self.server.fname_get_completed = True
        super().do_GET()

    def do_POST(self):
        self.send_error(404, "File not found")


def open_trace(path, open_browser=True):
    # We reuse the HTTP+RPC port because it's the only one allowed by the CSP.
    PORT = 9001
    HOST = "https://ui.perfetto.dev"
    
    path = os.path.abspath(path)
    os.chdir(os.path.dirname(path))
    fname = os.path.basename(path)

    socketserver.TCPServer.allow_reuse_address = True

    with socketserver.TCPServer(("127.0.0.1", PORT), HttpHandler) as httpd:
        address = f"{HOST}/#!/?url=http://127.0.0.1:{PORT}/{fname}&referrer=open_trace_in_ui"

        if open_browser:
            chrome_path = '/Applications/Google Chrome.app'
            if os.path.exists(chrome_path):
                open_chrome_path = "open -a /Applications/'Google Chrome'.app %s"
                webbrowser.get(open_chrome_path).open_new_tab(address)
            else:
                webbrowser.open_new_tab(address)
        else:
            print(f"Open URL in browser: {address}")

        httpd.expected_fname = fname
        httpd.fname_get_completed = None
        httpd.allow_origin = HOST
        
        while httpd.fname_get_completed is None:
            httpd.handle_request()


# if __name__ == "__main__":
#     trace_file = "/Users/bytedance/1733466284.pb"
#     open_trace(trace_file)
