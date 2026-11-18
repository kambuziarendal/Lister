# SPDX-License-Identifier: MIT
"""Run with the compiled ftp-home probe and pyftpdlib on the same host."""
import pathlib
import subprocess
import sys
import tempfile
import threading

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer


class Handler(FTPHandler):
    def on_login(self, username):
        self.fs.cwd = "/home"


with tempfile.TemporaryDirectory() as sandbox:
    root = pathlib.Path(sandbox)
    home = root / "home"
    home.mkdir()
    (home / "visible").write_text("shown")
    (root / "visible").write_text("unrelated")
    Handler.authorizer = DummyAuthorizer()
    Handler.authorizer.add_user("fixture", "fixture", sandbox, perm="elradfmwMT")
    server = FTPServer(("127.0.0.1", 0), Handler)
    port = server.socket.getsockname()[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        subprocess.run([sys.argv[1], f"ftp://127.0.0.1:{port}/"], check=True, timeout=30)
        assert (root / "visible").read_text() == "unrelated"
        assert not list(home.iterdir())
    finally:
        server.close_all()
