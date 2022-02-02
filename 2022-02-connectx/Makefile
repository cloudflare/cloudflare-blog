
.PHONY: test
test: venv/.ok
	unshare -Ur ./venv/bin/coverage run -m unittest test.py
	./venv/bin/coverage report -m

.PHONY: format
format:
	./venv/bin/yapf -i test_base.py
	./venv/bin/yapf -i test.py
	./venv/bin/yapf -i connectx.py


venv/.ok:
	virtualenv venv
	./venv/bin/pip3 install yapf
	./venv/bin/pip3 install coverage
	touch $@


