import os
import sys
import logging
from datetime import datetime
import json

from fastapi import FastAPI, Request
from fastapi.encoders import jsonable_encoder
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from pymongo import MongoClient

# logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# start instance
app = FastAPI()
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")   #start jinja it'll find the templates folder

# pymongo configuration
mongo_host = os.getenv('MONGO_HOST', None)
if mongo_host is None:
    logging.error('MONGO_HOST undefined.')
    sys.exit(1)
mongo_port = os.getenv('MONGO_PORT', None)
if mongo_port is None:
    logging.error('MONGO_PORT undefined.')
    sys.exit(1)
mongo_client = MongoClient(mongo_host, int(mongo_port))

@app.get('/register')
async def register(request: Request):
    return templates.TemplateResponse(
        request=request, name="index.html", context={"dummy": 0}
    )



async def request_data(data):
    req_key = ["user_name", "user_phone", "user_address"]
    for key in req_key:
        if key not in data:
            return False,key
    return True, None

@app.post('/api/register')
async def register(request: Request):
    user_db = mongo_client.user_db
    user_col = user_db.users
    data = await request.json()
    # Verify request data
    is_valid,missing_key = await request_data(data)
    if not is_valid:
        resp = {'missing': missing_key}
        return jsonable_encoder(resp)
    # Add timestamp to the data
    data["timestamp"] = datetime.now()

    # Insert data into the database
    result = user_col.insert_one(data)
    # resp = {"status": "OK","user_id":str(result.insert_id)}
    resp ={"status": "OK"}

    return jsonable_encoder(resp)
