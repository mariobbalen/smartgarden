import os

import requests
from dotenv import load_dotenv
from flask import Flask, jsonify, render_template

load_dotenv()

SUPABASE_URL = os.environ["SUPABASE_URL"]
SUPABASE_KEY = os.environ["SUPABASE_KEY"]
TABLE_NAME = "sensor_data"
READINGS_LIMIT = 50

app = Flask(__name__)


def fetch_readings(limit=READINGS_LIMIT):
    url = f"{SUPABASE_URL}/rest/v1/{TABLE_NAME}"
    params = {"select": "*", "order": "recorded_at.desc", "limit": limit}
    headers = {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
    }
    response = requests.get(url, params=params, headers=headers, timeout=10)
    response.raise_for_status()
    return response.json()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/readings")
def api_readings():
    return jsonify(fetch_readings())


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
