import urllib.request
localhost = urllib.request.urlopen("http://localhost:8000/api/list")
print (localhost.read(100))