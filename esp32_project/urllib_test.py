import urllib.request
import json
from collections import defaultdict


def fetch_data(url):
    with urllib.request.urlopen(url) as response:
        data = json.loads(response.read().decode())
    return data


users_url = 'https://jsonplaceholder.typicode.com/users'
users = fetch_data(users_url)

posts_url = 'https://jsonplaceholder.typicode.com/posts'
posts = fetch_data(posts_url)

user_posts = defaultdict(list)
for post in posts:
    user_id = post['userId']
    user_posts[user_id].append(post)

posts_per_user = {user['name']: len(user_posts[user['id']]) for user in users}

for user_name, post_count in posts_per_user.items():
    print(f"{user_name}: {post_count} posts")