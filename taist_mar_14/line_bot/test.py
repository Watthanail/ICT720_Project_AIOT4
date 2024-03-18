import urllib.request
import json
#from collections import defaultdict


def fetch_data(url):
    try:
        # Make HTTP request to localhost API
        response = urllib.request.urlopen(url)
        # Read the response content
        response_content = response.read()
        # Decode the response content from bytes to string
        response_content_str = response_content.decode('utf-8')
        # Parse the JSON data
        data = json.loads(response_content_str)
        return data
    except urllib.error.HTTPError as e:
        print("HTTP Error:", e)
    except urllib.error.URLError as e:
        print("URL Error:", e)
    except Exception as e:
        print("Error occurred during HTTP request:", e)
    return None

users_url = 'http://localhost:8000/api/list'
users = fetch_data(users_url)

print("Response from localhost API:", users)
