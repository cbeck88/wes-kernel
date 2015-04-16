EnsureSConsVersion(0,98,3)

import os, sys, shutil, re, commands
from glob import glob
from subprocess import Popen, PIPE, call
from os import access, F_OK

# Warn user of current set of build options.
AddOption('--option-cache', dest='option_cache', nargs=1, type = 'string', action = 'store', metavar = 'FILE', help='file with cached construction variables', default = '.scons-option-cache')
if os.path.exists(GetOption("option_cache")):
    optfile = file(GetOption("option_cache"))
    print "Saved options:", optfile.read().replace("\n", ", ")[:-2]
    optfile.close()

opts = Variables(GetOption("option_cache"))

opts.AddVariables(
	ListVariable('default_targets', 'Targets that will be built if no target is specified in command line.',
		"wml,kernel", Split("wml kernel attr attr2 kv")),
	PathVariable('build_dir', 'Build all intermediate files(objects, test programs, etc) under this dir', "build", PathVariable.PathAccept),
	('extra_flags_config', 'Extra compiler and linker flags to use for configuration and all builds', ""),
	PathVariable('bindir', 'Where to install binaries', "bin", PathVariable.PathAccept),
	('cachedir', 'Directory that contains a cache of derived files.', ''),
	('cxxtool', 'Set c++ compiler command if not using standard compiler.'),
	('host', 'Cross-compile host.', ''),
	('jobs', 'Set the number of parallel compilations', "1", lambda key, value, env: int(value), int),
	PathVariable('prefix', 'autotools-style installation prefix', "/usr/local", PathVariable.PathAccept),
	BoolVariable('strict', 'Set to strict compilation', True),
	BoolVariable("fast", "Make scons faster at cost of less precise dependency tracking.", False),
	BoolVariable("lockfile", "Create a lockfile to prevent multiple instances of scons from being run at the same time on this working copy.", False),
	BoolVariable("OS_ENV", "Forward the entire OS environment to scons", False),
	BoolVariable('ccache', "Use ccache", False))


#
# Setup
#

toolpath = ["scons"]
for repo in Dir(".").repositories:
  # SCons repositories are additional dirs to look for source and lib files.
  # It is possible to make out of tree builds by running SCons outside of this
  # source code root and supplying this path with -Y option.
  toolpath.append(repo.abspath + "/scons")
sys.path = toolpath + sys.path
env = Environment(options = opts, toolpath = toolpath)
#env = Environment(tools=["tar", "gettext", "install", "g++" ], options = opts, toolpath = toolpath)

if env["lockfile"]:
    print "Creating lockfile"
    lockfile = os.path.abspath("scons.lock")
    open(lockfile, "wx").write(str(os.getpid()))
    import atexit
    atexit.register(os.remove, lockfile)

opts.Save(GetOption("option_cache"), env)
env.SConsignFile("$build_dir/sconsign.dblite")

# If OS_ENV was enabled, copy the entire OS environment.
if env['OS_ENV']:
    env['ENV'] = os.environ

# Make sure the user's environment is always available
env['ENV']['PATH'] = os.environ.get("PATH")
term = os.environ.get('TERM')
if term is not None:
    env['ENV']['TERM'] = term

if env.get('cxxtool',""):
    env['CXX'] = env['cxxtool']
    if 'HOME' in os.environ:
        env['ENV']['HOME'] = os.environ['HOME']

if env['jobs'] > 1:
    SetOption("num_jobs", env['jobs'])

if env['ccache']: env.Tool('ccache')

if env["cachedir"] and not env['ccache']:
    CacheDir(env["cachedir"])

#
# Check some preconditions
#
print "---[checking prerequisites]---"

def Info(message):
    print message
    return True

def Warning(message):
    print message
    return False

from metasconf import init_metasconf
configure_args = dict(
    custom_tests = init_metasconf(env, ["cplusplus", "boost", "pkgconfig"]),
    config_h = "$build_dir/config.h",
    log_file="$build_dir/config.log", conf_dir="$build_dir/sconf_temp")

env.MergeFlags(env["extra_flags_config"])

#env.PrependENVPath('LD_LIBRARY_PATH', env["boostlibdir"])

conf = env.Configure(**configure_args)

have_prereqs = (conf.CheckCPlusPlus(gcc_version = "3.3") & \
	conf.CheckBoost("iostreams", require_version = "1.34.1") & \
	conf.CheckBoostIostreamsGZip() & \
	conf.CheckBoostIostreamsBZip2() & \
	conf.CheckBoost("random",require_version = "1.40.0") & \
	conf.CheckBoost("smart_ptr", header_only = True) & \
	conf.CheckBoost("system") & \
	conf.CheckBoost("filesystem", require_version = "1.44.0") \
            and Info("GOOD: Base prerequisites are met")) \
            or Warning("WARN: Base prerequisites are not met")

print "  " + env.subst("If any config checks fail, look in $build_dir/config.log for details")
print "  If a check fails spuriously due to caching, use --config=force to force its rerun"


#
# Implement configuration switches
#
print "---[applying configuration]---"

build_root="#/"
env.Prepend(CPPPATH = [build_root + "$build_dir", "#/src"])

env.Append(CPPDEFINES = ["HAVE_CONFIG_H"])

if "gcc" in env["TOOLS"]:
    env.AppendUnique(CCFLAGS = Split("-W -Wall -std=c++11"))

    if env['strict']:
        env.AppendUnique(CCFLAGS = Split("-Werror $(-Wno-unused-local-typedefs$)"))


env = conf.Finish()
client_env = env.Clone()


Export(Split("env client_env"))

build_dir = os.path.join("$build_dir", "build")

env.SConscript("src/SConscript", variant_dir = build_dir, duplicate = False)

binaries = Split("wml json test attr attr2 nl kv kernel_test")
#Import(binaries + ["sources"])

all = env.Alias("all", map(Alias, binaries))
env.Default(map(Alias, env["default_targets"]))

