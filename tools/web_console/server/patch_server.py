with open('/home/pchanda/MeshX/tools/web_console/server/server.py', 'r') as f:
    content = f.read()

old_server = """                    self.broadcast({
                        "type": "nodes_discovered",
                        "nodes": discovered_nodes
                    })
                except Exception as e:
                    logger.error(f"Error parsing Dynamic Composition response: {e}")
        elif msg_type == 0x90:"""

new_server = """                    self.broadcast({
                        "type": "nodes_discovered",
                        "nodes": discovered_nodes
                    })
                except Exception as e:
                    logger.error(f"Error parsing Dynamic Composition response: {e}")
        elif msg_type == 0x87:
            if len(payload) >= 1:
                try:
                    num_elements = payload[0]
                    offset = 1
                    for _ in range(num_elements):
                        if offset + 6 > len(payload):
                            break
                        idx, variant, ctx_size = struct.unpack("<HHH", payload[offset:offset+6])
                        offset += 6
                        
                        data = bytes()
                        if ctx_size > 0 and offset + ctx_size <= len(payload):
                            data = payload[offset:offset+ctx_size]
                        
                        offset += ctx_size
                        
                        address = f"0x00{idx:02X}"
                        if address in self.state_cache["nodes"]:
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
                            })
                except Exception as e:
                    logger.error(f"Error parsing Element State Response: {e}")
        elif msg_type == 0x90:"""

content = content.replace(old_server, new_server)
with open('/home/pchanda/MeshX/tools/web_console/server/server.py', 'w') as f:
    f.write(content)
