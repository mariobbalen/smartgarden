import os

import requests
from dotenv import load_dotenv
from flask import Flask, jsonify, render_template, request

load_dotenv()

SUPABASE_URL = os.environ["SUPABASE_URL"]
SUPABASE_KEY = os.environ["SUPABASE_KEY"]
TABLE_NAME = "sensor_data"
CONFIG_TABLE_NAME = "device_config"
READINGS_LIMIT = 50

app = Flask(__name__)


def fetch_table(table_name, **params):
    url = f"{SUPABASE_URL}/rest/v1/{table_name}"
    headers = {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
    }
    response = requests.get(url, params=params, headers=headers, timeout=10)
    response.raise_for_status()
    return response.json()


def fetch_readings(limit=READINGS_LIMIT):
    return fetch_table(TABLE_NAME, select="*", order="recorded_at.desc", limit=limit)


def fetch_config():
    return fetch_table(CONFIG_TABLE_NAME, select="*", order="sector.asc")


def update_config(sector, fields):
    url = f"{SUPABASE_URL}/rest/v1/{CONFIG_TABLE_NAME}"
    params = {"sector": f"eq.{sector}"}
    headers = {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
        "Content-Type": "application/json",
        "Prefer": "return=representation",
    }
    response = requests.patch(url, params=params, headers=headers, json=fields, timeout=10)
    response.raise_for_status()
    return response.json()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/readings")
def api_readings():
    return jsonify(fetch_readings())


@app.route("/api/config")
def api_config():
    return jsonify(fetch_config())


@app.route("/api/config/<int:sector>", methods=["PATCH"])
def api_update_config(sector):
    fields = request.get_json()
    return jsonify(update_config(sector, fields))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
