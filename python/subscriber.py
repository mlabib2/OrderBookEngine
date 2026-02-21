import redis

# Connect to Redis (same default host/port as the C++ engine)
client = redis.Redis(host="127.0.0.1", port=6379)
pubsub = client.pubsub()

# Subscribe to the trades channel
pubsub.subscribe("trades")
print("Listening for trades on channel 'trades'...")

# Print every message received
for message in pubsub.listen():
    if message["type"] == "message":
        print(f"Trade received: {message['data'].decode()}")
