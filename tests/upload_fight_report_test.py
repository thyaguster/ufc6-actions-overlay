import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
from threading import Thread
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from upload_fight_report import send_report


class Handler(BaseHTTPRequestHandler):
    captures = []

    def do_POST(self):
        assert self.path == '/api/bridge/game-capture'
        assert self.headers['Authorization'] == 'Bearer ptg_ovl_' + 'x' * 24
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.captures.append(body['capture'])
        answer = json.dumps({'captureId': body['capture']['id'], 'matchId': 'test-match'}).encode()
        self.send_response(201)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(answer)))
        self.end_headers()
        self.wfile.write(answer)

    def log_message(self, *_args):
        pass


class UploadTest(unittest.TestCase):
    def test_result_then_complete_and_idempotence(self):
        Handler.captures = []
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        worker = Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            with TemporaryDirectory() as folder:
                directory = Path(folder)
                fight_id = '8a3ab990-e0eb-4e2b-bddb-7e28430bfdf0'
                token = 'ptg_ovl_' + 'x' * 24
                url = f'http://127.0.0.1:{server.server_port}'
                for stage, name in [('result', 'fight-result.json'), ('complete', 'fight-complete.json')]:
                    report = directory / name
                    report.write_text(json.dumps({'schema': 1, 'id': fight_id,
                                                  'reportStage': stage, 'result': {'winnerCorner': 'red'}}))
                    self.assertEqual(send_report(report, url, token)['matchId'], 'test-match')
                    send_report(report, url, token)
                self.assertEqual([x['reportStage'] for x in Handler.captures], ['result', 'complete'])
        finally:
            server.shutdown()
            worker.join()
            server.server_close()


if __name__ == '__main__':
    unittest.main()
