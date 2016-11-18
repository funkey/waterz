default:
	rm -r build dist || true
	rm -rf ~/.miniconda/envs/caffe/lib/python2.7/site-packages/waterz-*.egg || true
	python setup.py install
