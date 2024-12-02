import re
import json

def parse_sdkconfig(file_path):
    config_dict = {}

    with open(file_path, 'r') as file:
        for line in file:
            # Remove any leading/trailing whitespace
            line = line.strip()
            
            # Skip empty lines or comments (lines starting with '#')
            if not line or line.startswith('#'):
                continue
            
            # Match a key-value pair (e.g., KEY=value)
            match = re.match(r"^([A-Za-z0-9_]+)\s*=\s*(.*)$", line)
            if match:
                key = match.group(1)
                value = match.group(2).strip()

                # Handle boolean values
                if value == 'y':
                    value = True
                elif value == 'n':
                    value = False
                else:
                    # Try to convert to integer if possible
                    try:
                        value = int(value)
                    except ValueError:
                        # If it's not an integer, leave it as a string
                        pass

                config_dict[key] = value
            else:
                print(f"Warning: Unrecognized line format: {line}")
                
    return config_dict

