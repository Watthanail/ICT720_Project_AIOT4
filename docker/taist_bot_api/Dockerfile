# Python runtime
FROM python:3.10

# Working directory
WORKDIR /code

# copy required libraries
COPY ./requirements.txt /code/requirements.txt

# install required libraries
RUN pip install --no-cache-dir --upgrade -r /code/requirements.txt

# copy all files
COPY . /code
COPY ./static/ /code/static
COPY ./templates/ /code/templates

# run code
#CMD ["uvicorn", "taist_bot_app:app", "--host", "0.0.0.0", "--port", "8003"]
# run code
CMD uvicorn taist_bot_app:app --host 0.0.0.0 --port ${LINE_WEBHOOK_PORT}