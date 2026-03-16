from flask import Flask, send_from_directory, request, abort
import os

app = Flask(__name__)
API_KEY = os.environ.get("API_KEY", "palepale_2026_key")
FILES_DIR = "/app/files"

@app.route("/health")
def health():
    return "ok"

@app.route("/download/<filename>")
def download(filename):
    if request.headers.get("X-API-Key") != API_KEY:
        abort(403)
    if ".." in filename or "/" in filename:
        abort(400)
    return send_from_directory(FILES_DIR, filename)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 8080)))
