import json
import yaml
import os
import sys

file_insert =   "/****************************************************************************\n"\
                " * AUTOGEN CODE\n"\
                "****************************************************************************/\n"\
                "#ifndef __AUTO_GEN__\n#define __AUTO_GEN__\n"

class CodeGenException (Exception):
    pass

class CodeGen(CodeGenException):
    def __init__(self,
                 target_name,
                 prod_profile = "scripts/prod_profile.json",
                 idf_yml = "main/idf_component.yml"):
        self.max_el = 1
        self.target = target_name
        with open(prod_profile) as pj:
            self.prod_profile = json.load(pj)
        self.product_list = []
        self.product = None
        self.pid = None
        self.cid = self.prod_profile['prod']['cid']
        self.deps = {}
        self.yml = idf_yml
        self.__find_prod_from_target()
        self.file_insert = file_insert
        self.genfile_h = "prod_common/common/codegen.h"
        self.define_fmt = "\n#define {} {}"
        os.system(f'touch {self.genfile_h}')

    def __str__(self):
        return self.file_insert

    def __find_prod_from_target(self):
        self.product_list = self.prod_profile['prod']['products']
        for product in self.product_list:
            if product['name'] == self.target:
                self.product = product
                self.pid = self.product['pid']
        if self.product is None:
            raise CodeGenException("Product Not Found")

    def __resolve_dep_create_yml_dict(self, dep_s, value:int = 0):
        for _idx, dep in enumerate(self.prod_profile['elements']):
            if dep["name"] == dep_s:
                self.deps[dep_s] = {'path' : dep['path']}
                if isinstance(dep['macro']['value'], bool):
                    self.prod_profile['elements'][_idx]['macro']['value'] = True
                else:
                    self.prod_profile['elements'][_idx]['macro']['value'] = value
                
                for sub_dep in dep['deps']:
                    self.__resolve_dep_create_yml_dict(sub_dep)

    def close(self):
        self.file_insert += "\n\n#endif /* __AUTO_GEN__ */"
        with open(self.genfile_h, 'w') as genfile:
            genfile.write(str(gen) + '\n')

    def get_common_defs(self):
        self.max_el = 1
        if self.product is not None:
            for element in self.product['elements']:
                self.max_el += element['value']
        self.file_insert += self.define_fmt.format("CONFIG_CID_ID", self.cid)
        self.file_insert += self.define_fmt.format("CONFIG_PID_ID", self.pid)
        self.file_insert += self.define_fmt.format("CONFIG_PRODUCT_NAME", f'"{self.target[:16:]}"')
        self.file_insert += self.define_fmt.format("CONFIG_MAX_ELEMENT_COUNT", self.max_el)
        return self.file_insert

    def resolve_el_deps(self):
        if self.product is not None:
            for element in self.product['elements']:
                self.__resolve_dep_create_yml_dict(element['name'], value=element['value'])
            with open(self.yml, 'w') as yml:
                yaml.dump({"dependencies" : self.deps}, yml)

    def get_el_macro(self):
        if self.product is not None:
            for element_reg in self.prod_profile['elements']:
                self.file_insert += self.define_fmt.format(element_reg['macro']['def'],
                                                            int(element_reg['macro']['value']))

if __name__ == '__main__':
    gen = CodeGen(sys.argv[1])
    gen.resolve_el_deps()
    gen.get_common_defs()
    gen.get_el_macro()
    gen.close()
    print(json.dumps(gen.deps, indent=4))

    print(f">> Autogen code created! \n{gen}")