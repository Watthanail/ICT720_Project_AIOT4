import os
import sys
import logging
import requests
from datetime import datetime

from fastapi import FastAPI, Request, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from linebot.v3.webhook import WebhookHandler
from linebot.v3.exceptions import InvalidSignatureError
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

# Logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Linebot configuration
channel_secret = os.getenv('LINE_CHANNEL_SECRET')
liff_id = os.getenv('LIFF_ID')
channel_access_token = os.getenv('LINE_ACCESS_TOKEN')
if None in (channel_secret, liff_id, channel_access_token):
    print('Specify LINE_CHANNEL_SECRET, LIFF_ID, and LINE_ACCESS_TOKEN as environment variables.')
    sys.exit(1)
configuration = Configuration(access_token=channel_access_token)

# Start FastAPI instance
app = FastAPI()
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")
handler = WebhookHandler(channel_secret)


@app.get('/')
async def liff_ui(request: Request):
    return templates.TemplateResponse(request=request, name="index.html", context={"LIFF_ID": liff_id})

