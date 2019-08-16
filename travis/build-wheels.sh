#!/bin/bash
set -e -x

# Install a system package required by our library
sudo apt install -y libboost-dev

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    sudo "${PYBIN}/pip" install -r /io/requirements.txt
    sudo "${PYBIN}/pip" wheel /io/ -w wheelhouse/
done

# Bundle external shared libraries into the wheels
for whl in wheelhouse/*.whl; do
    sudo auditwheel repair "$whl" --plat $PLAT -w /io/wheelhouse/
done

# Install packages and test
for PYBIN in /opt/python/*/bin/; do
    sudo "${PYBIN}/pip" install waterz --no-index -f /io/wheelhouse
done
