PYPI_REPO := testpypi

all: clean install wheel

configure:
	cp -rf patches/gnubg-nn/* gnubg-nn/
	cd gnubg-nn && autoreconf -i &&	./configure

clean:
	rm -rf build/ dist/ analyze/
	rm -rf gnubg/gnubg.egg-info

install:
	pip install --upgrade pip setuptools wheel cibuildwheel twine

wheel:
	python3 setup.py sdist bdist_wheel

repair: wheel
	auditwheel repair dist/*.whl \
      --plat manylinux2014_x86_64 \
      -w dist

twine: repair
	twine upload --verbose --repository $(PYPI_REPO) dist/*manylinux*.whl

test:
	pip install --force-reinstall dist/*.whl
	python3 tests/test.py

# Check-in code after formatting
checkin: ## Perform a check-in after formatting the code
    ifndef COMMIT_MESSAGE
		$(eval COMMIT_MESSAGE := $(shell bash -c 'read -e -p "Commit message: " var; echo $$var'))
    endif
	@git add --all; \
	  git commit -m "$(COMMIT_MESSAGE)"; \
	  git push
