PYPI_REPO := testpypi

all: clean install patch configure build test

configure:
	cd gnubg-nn && autoreconf -i &&	./configure

clean:
	rm -rf build/ dist/ analyze/
	rm -rf gnubg/gnubg.egg-info

install:
	pip install --upgrade pip setuptools wheel cibuildwheel twine

build:
	python3 setup.py sdist bdist_wheel

wheel:
	cibuildwheel --platform linux --output-dir dist

twine:
	twine upload --verbose --repository $(PYPI_REPO) dist/*manylinux*.whl

test:
	pip install --force-reinstall dist/*.whl
	python3 src/gnubg/tests/test.py

patch:
	cp -rf patches/gnubg-nn/* gnubg-nn/

# Check-in code after formatting
checkin: ## Perform a check-in after formatting the code
    ifndef COMMIT_MESSAGE
		$(eval COMMIT_MESSAGE := $(shell bash -c 'read -e -p "Commit message: " var; echo $$var'))
    endif
	@git add --all; \
	  git commit -m "$(COMMIT_MESSAGE)"; \
	  git push
