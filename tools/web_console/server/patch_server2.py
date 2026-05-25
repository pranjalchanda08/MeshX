import re

with open('/home/pchanda/MeshX/tools/web_console/server/server.py', 'r') as f:
    content = f.read()

# Replace the specific block of code handling 0x87 state broadcasts
# It looks like:
#                             if len(data) >= 4:
#                                 # First byte as a simple state value indicator
#                                 val = data[0]
#                             self.state_cache["nodes"][address]["value"] = val
#                             
#                             self.broadcast({
#                                 "type": "node_state_update",
#                                 "address": address,
#                                 "value": val,
#                                 "element_idx": idx,
#                                 "element_type": variant,
#                                 "func_id": 0,
#                                 "data_hex": data.hex().upper()
#                             })

old_block = """                        if address in self.state_cache["nodes"]:
                            val = 0
                            if len(data) >= 4:
                                # First byte as a simple state value indicator
                                val = data[0]
                            self.state_cache["nodes"][address]["value"] = val
                            
                            self.broadcast({
                                "type": "node_state_update",
                                "address": address,
                                "value": val,
                                "element_idx": idx,
                                "element_type": variant,
                                "func_id": 0,
                                "data_hex": data.hex().upper()
                            })"""

new_block = """                        if address in self.state_cache["nodes"]:
                            pass # 0x87 context is NVS context (app_id, pub_addr), not telemetry state."""

content = content.replace(old_block, new_block)

with open('/home/pchanda/MeshX/tools/web_console/server/server.py', 'w') as f:
    f.write(content)
