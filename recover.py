import json

log_path = r"C:\Users\666\.gemini\antigravity\brain\fb892c9d-fc88-4d91-ae13-ce4dded052b8\.system_generated\logs\overview.txt"

with open(log_path, 'r', encoding='utf-8') as f:
    for line in f:
        try:
            data = json.loads(line.strip())
            source = data.get("source")
            step_idx = data.get("step_index")
            
            # 如果是 SYSTEM 或者带有 output
            if "output" in data or source == "SYSTEM":
                print(f"Found SYSTEM step {step_idx} | Keys: {list(data.keys())}")
                output = data.get("output", "")
                if output:
                    print(f"  -> output size: {len(output)} bytes. Snippet:")
                    print(output[:200])
                    print("-" * 40)
        except Exception as e:
            pass
