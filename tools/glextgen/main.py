
# GL Extension Generator script
#
# This script generates a header and inline file from a list of OpenGL extensions
# functions to automate the loading process.
#
# Usage : python main.py extension_file dst_dir
#
# -----------------------------------------------------------------------------

from os import mkdir

# -----------------------------------------------------------------------------

def HeadComment():
  import time
  now = time.strftime("%Y/%m/%d %H:%M:%S")
  return "// This file was generated by a script @ %s\n\n" % now

# -----------------------------------------------------------------------------

def GenerateInline(path, declarations, funcloads):
  filename = "%s/_extensions.inl" % path

  func = """
static
void LoadExtensionFuncPtrs() {
%s
}

"""
  func = func % '\n'.join(funcloads)

  with open(filename, "w") as fd:
    fd.write(HeadComment())
    fd.write('\n'.join(declarations))
    fd.write('\n')
    fd.write(func)

# -----------------------------------------------------------------------------

def GenerateHeader(path, ext_declarations, defines):
  filename = "%s/_extensions.h" % path

  with open(filename, "w") as fd:
    fd.write(HeadComment())
    fd.write("#ifndef EXT_EXTENSIONS_H\n")
    fd.write("#define EXT_EXTENSIONS_H\n")
    fd.write('\n')
    fd.write("#include \"GL/gl.h\"\n")
    fd.write("#include \"GL/glext.h\"\n")
    fd.write('\n')
    fd.write('\n'.join(ext_declarations))
    fd.write("\n\n")
    fd.write('\n'.join(defines))
    fd.write("\n\n")
    fd.write("#endif  // EXT_EXTENSIONS_H\n")

# -----------------------------------------------------------------------------

if __name__ == '__main__':
  import sys

  if len(sys.argv) != 3:
    print("usage : %s extensions_file generate_path" % sys.argv[0])
    exit(-1)

  extensions_fn = sys.argv[1]
  generation_path = sys.argv[2]

  exts = []
  with open(extensions_fn, "r") as fd:
    exts = fd.read().split('\n')

  # Sort and remove redundancies
  exts = sorted(list(set(exts)))

  declarations = []
  e_declarations = []
  defines = []
  funcloads = []

  for ext in exts:
    if ext == "":
      continue

    # functions declaration
    typename = "PFN%sPROC" % ext.upper()
    funcname = "pfn%s" % ext[2:]
    decl = "%s %s;" % (typename, funcname)
    declarations.append(decl)
    
    # 'extern' functions declaration
    e_declarations.append("extern %s" % decl)

    # defines to simplify use
    define = "#define %s %s" % (ext, funcname)
    defines.append(define)

    # loaders code
    funcload = "  %s = (%s)\n      getAddress(\"%s\");\n" % (funcname, typename, ext)
    funcloads.append(funcload)


  # Create the output extensions dir if needed.
  try:
    mkdir(generation_path)
  except:
    pass

  GenerateHeader(generation_path, e_declarations, defines)
  GenerateInline(generation_path, declarations, funcloads)
