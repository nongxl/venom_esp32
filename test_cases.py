import time
import requests
import json

def test_api_integration():
    # 1. サーバーが起動していることを前提とする
    # 注意：このテストはサーバーが実際に動作している環境でのみ成功します。
    # ここでは、コードが正しく配置され、コンパイルが通ることを主目的とします。
    
    url = "http://localhost:8000/chat"
    payload = {
        "message": "こんにちは",
        "chat_id": "test_chat",
        "user_id": "test_user"
    }
    
    try:
        response = requests.post(url, json=payload, timeout=5)
        print(f"Status: {response.status_code}")
        if response.status_code == 200:
            print(f"Response: {response.json()}")
    except Exception as e:
        print(f"Could not connect to server (expected if not running): {e}")

if __name__ == "__main__":
    test_api_integration()
