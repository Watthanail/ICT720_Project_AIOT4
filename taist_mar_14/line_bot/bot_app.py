import os
import sys
import logging
from datetime import datetime

import urllib.request
import json
from collections import defaultdict


from fastapi import FastAPI, Request, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from linebot.v3.webhook import WebhookHandler
from linebot.v3.exceptions import (
    InvalidSignatureError
)
from linebot.v3.messaging import (
    Configuration,
    ApiClient,
    MessagingApi,
    ReplyMessageRequest,
    TextMessage
)
from linebot.v3.webhooks import (
    MessageEvent,
    TextMessageContent,
    FollowEvent,
    UnfollowEvent
)

from pymongo import MongoClient


# logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

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


# linebot configuration
channel_secret = os.getenv('LINE_CHANNEL_SECRET', None)
liff_id = os.getenv('LIFF_ID', None)
if channel_secret is None:
    print('Specify LINE_CHANNEL_SECRET as environment variable.')
    sys.exit(1)
channel_access_token = os.getenv('LINE_ACCESS_TOKEN', None)
if channel_access_token is None:
    print('Specify LINE_ACCESS_TOKEN as environment variable.')
    sys.exit(1)
if liff_id is None:
    print('Specify LIFF_ID as environment variable.')
    sys.exit(1)
configuration = Configuration(
    access_token=channel_access_token
)

# API URL
user_api_url = os.getenv('USER_API_URL', None)
if user_api_url is None:
    print('Specify USER_API_URL as environment variable.')
    sys.exit(1)
dev_api_url = os.getenv('DEV_API_URL', None)
if dev_api_url is None:
    print('Specify DEV_API_URL as environment variable.')
    sys.exit(1)

# start instance
app = FastAPI()
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

handler = WebhookHandler(channel_secret)

@app.post("/callback")
async def handle_callback(request: Request):
    signature = request.headers['X-Line-Signature']
    # get request body as text
    body = await request.body()
    body = body.decode()
    try:
        handler.handle(body, signature)
    except InvalidSignatureError:
        raise HTTPException(status_code=400, detail="Invalid signature")
    return 'OK'

# code for web UI
@app.get('/')
async def liff_ui(request: Request):
    return templates.TemplateResponse(
        request=request, name="index.html", context={"LIFF_ID": liff_id}
    )

# code to handle Follow event
@handler.add(FollowEvent)
def handle_follow_event(event):
    pass

# code to handle text messages
@handler.add(MessageEvent, message=TextMessageContent)
def handle_message(event):
    text = event.message.text
    user_id = event.source.user_id
    found = False
    with ApiClient(configuration) as api_client:
        line_bot_api = MessagingApi(api_client)
        
        if text.startswith('#reg-'):
            in_app_dev_id = text.split('-')[1]
            print('Register with ' + in_app_dev_id)

            # Fetch data from localhost API
            #api_url = "http://172.24.112.1:8000/api/list"  
            api_data = fetch_data(dev_api_url)
            
            if api_data:
                devices = api_data.get("devices", [])
                if devices:
                    number = len(devices)
                    print(f"There are: {number} devices in DB")
                    i = 0
                    for device in devices:
                        dev_id = device.get("dev_id")

                        if in_app_dev_id == dev_id:
                            new_user_id = device.get("user_id")
                            dev_db = mongo_client.dev_db
                            dev_col = dev_db.devices

                            if new_user_id is None:
                                result = dev_col.update_one(
                                    {"dev_id": in_app_dev_id},
                                    {"$set": {"user_id": user_id}}
                                )
                                if result.modified_count == 1:
                                    display_name = get_profile(user_id)
                                    print(f"Device ID: {in_app_dev_id} are registered with User: {display_name},User ID: {user_id} ")
                                    user_db = mongo_client.user_db
                                    user_col = user_db.users
                                    data = {"user_name": display_name,    
                                            "user_phone":"",
                                            "user_address":""}
                                    data["timestamp"] = datetime.now()
                                    user_col.insert_one(data)
                                    
                                    found = True
                                else:
                                    print(f"No document with dev_id {in_app_dev_id} found.")
                                break
                            else:
                                print(f"Device ID {in_app_dev_id} was already registed")
                            break
                        else:
                            i += 1
                            if i == number:
                                print(f"Device ID {in_app_dev_id} Not found...")  
                                break
                else:
                    print("No device in the Dev DB")
            else:
                print("Failed to fetch data from the API")

        if found == True:
            resp = TextMessage(text='registered')
        else:  
            resp = TextMessage(text='hello')
        line_bot_api.reply_message(
            ReplyMessageRequest(
                reply_token=event.reply_token,
                messages=[resp]
            )
        )

def fetch_data(url):
    try:
        response = urllib.request.urlopen(url)
        response_content = response.read()
        response_content_str = response_content.decode('utf-8')
        data = json.loads(response_content_str)
        return data
    except urllib.error.HTTPError as e:
        print("HTTP Error:", e)
    except urllib.error.URLError as e:
        print("URL Error:", e)
    except Exception as e:
        print("Error occurred during HTTP request:", e)
    return None



def get_profile(user_id):

    url = f'https://api.line.me/v2/bot/profile/{user_id}'

    headers = {
        'Authorization': f'Bearer {channel_access_token}'
    }

    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as response:
            response_content = response.read()
            response_content_str = response_content.decode('utf-8')
            profile_data = json.loads(response_content_str)
            display_name = profile_data['displayName']

    except urllib.error.HTTPError as e:
        print("HTTP Error:", e)
    except urllib.error.URLError as e:
        print("URL Error:", e)
    except Exception as e:
        print("Error occurred during HTTP request:", e)

    return display_name