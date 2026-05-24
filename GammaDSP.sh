set -eu

PYTHON="${PYTHON:-python}"

py_include="$($PYTHON -c 'import sysconfig; print(sysconfig.get_paths()["include"])')"
py_platinclude="$($PYTHON -c 'import sysconfig; print(sysconfig.get_paths().get("platinclude") or sysconfig.get_paths()["include"])')"
numpy_include="$($PYTHON -c 'import numpy; print(numpy.get_include())')"
extsuffix="$($PYTHON -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')"
py_cflags="$($PYTHON -c 'import sysconfig; print(" ".join(x for x in [sysconfig.get_config_var("CFLAGS"), sysconfig.get_config_var("CCSHARED")] if x))')"
py_ldflags="$($PYTHON -c 'import sysconfig; print(" ".join(x for x in [sysconfig.get_config_var("LDFLAGS"), sysconfig.get_config_var("LIBS"), sysconfig.get_config_var("SYSLIBS")] if x))')"

echo "PYTHON=$($PYTHON -c 'import sys; print(sys.executable)')"
echo "EXT_SUFFIX=$extsuffix"
echo "PY_INCLUDE=$py_include"
echo "NUMPY_INCLUDE=$numpy_include"

swig -c++ -Iinclude -python GammaDSP.i

g++ -std=c++17 -fPIC -fmax-errors=1 -shared -O2 -D SWIG \
  -I. \
  -Iinclude \
  -Iinclude/GDSP \
  -I"$py_include" \
  -I"$py_platinclude" \
  -I"$numpy_include" \
  -I/usr/local/include \
  -I/usr/local/include/Gamma \
  -I/usr/local/include/r8brain \
  GammaDSP.cpp \
  GammaDSP_wrap.cxx \
  $py_cflags \
  -L/usr/local/lib -lGamma -lportaudio \
  $py_ldflags \
  -o "_GammaDSP${extsuffix}"
