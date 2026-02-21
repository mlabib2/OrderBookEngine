import redis
import os

REDIS_HOST = os.environ.get("REDIS_HOST", "127.0.0.1")

# Connect to Redis — host read from env so Docker and local both work
client = redis.Redis(host=REDIS_HOST, port=6379)
pubsub = client.pubsub()

# Subscribe to the trades channel
pubsub.subscribe("trades")
print("Listening for trades on channel 'trades'...")

# Print every message received
for message in pubsub.listen():
    if message["type"] == "message":
        print(f"Trade received: {message['data'].decode()}")
