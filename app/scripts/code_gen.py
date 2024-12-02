import parse_sdkconfig as ps
import json
import os

file_insert = "/****************************************************************************\n"\
" * AUTOGEN CODE\n"\
"****************************************************************************/\n"

class CodeGen:
    def __init__(self, sdkconfig_path):
        self.config = ps.parse_sdkconfig(sdkconfig_path)
        self.max_el = 1
        self.el_list_cfg = [
            "CONFIG_RELAY_SERVER_ELEMENT_NOS",
            "CONFIG_RELAY_CLIENT_ELEMENT_NOS"
        ]
        self.genfile_h = "prod_common/common/codegen.h"
        os.system(f"touch {self.genfile_h}")
    
    def __str__(self):
        ret = json.dumps(self.config, indent=4)
        ret += f"\nCONFIG_MAX_ELEMENT_COUNT = {self.max_el}"
        return ret

    def get_max_el_str(self):
        self.max_el = 1
        for cfg in self.el_list_cfg:
            try:
                self.max_el += self.config[cfg]
            except KeyError:
                print(f"Key {cfg} not found")
        return f"\n#define CONFIG_MAX_ELEMENT_COUNT {self.max_el}"

if __name__ == '__main__':
    gen = CodeGen('sdkconfig')
    file_insert += gen.get_max_el_str()
    with open(gen.genfile_h, 'w') as genfile:
        genfile.write(file_insert + '\n')
    print(">> Autogen code created!")