<h1 align="center">
<img src="img/gnubg_logo.png" width="300">
</h1><br>

[![PyPI Downloads](https://img.shields.io/pypi/dm/gnubg-nn-pypi.svg?label=PyPI%20downloads)](https://pypi.org/project/gnubg-nn-pypi/)
[![Conda Downloads](https://img.shields.io/conda/dn/conda-forge/gnubg-nn-pypi.svg?label=Conda%20downloads)](https://anaconda.org/conda-forge/gnubg-nn-pypi/)
[![GitHub issues](https://img.shields.io/github/issues/gnubg/gnubg-nn-pypi.svg)](https://github.com/gnubg/gnubg-nn-pypi/issues)
[![License](https://img.shields.io/badge/license-GPL%20v2-blue.svg)](#license)
[![Stack Overflow](https://img.shields.io/badge/stackoverflow-Ask%20questions-blue.svg)](https://stackoverflow.com/questions/tagged/gnubg)

GNUBG NeuralNet Python bindings bring the powerful GNUBG backgammon neural-network engine to Python 3.

* **Website:** [https://gnubg.org](https://gnubg.org)
* **Documentation:** [https://gnubg.org/doc](https://gnubg.org/doc)
* **Mailing list:** [https://lists.gnu.org/mailman/listinfo/gnubg](https://lists.gnu.org/mailman/listinfo/gnubg)
* **Source code:** [https://github.com/gnubg/gnubg-nn-pypi](https://github.com/gnubg/gnubg-nn-pypi)
* **Contributing:** [https://github.com/gnubg/gnubg-nn-pypi/blob/main/CONTRIBUTING.md](https://github.com/gnubg/gnubg-nn-pypi/blob/main/CONTRIBUTING.md)
* **Bug reports:** mailto\:bug-gnubg\@gnu.org
* **Report a security vulnerability:** mailto\:security\@gnubg.org

It provides:

* **Engine initialization & data loading** (neural-net weights, opening-book, bear-off tables)
* **Position classification** (`classify`) & **public-evaluation best move** (`pubbestmove`)
* **Board ↔ ID conversions** (`boardfromid`, `boardfromkey`, `keyofboard`, `id`)
* **Dice utilities** (`roll`) & **cube utilities** (`bestmove`, `pubevalscore`)
* **Bear-off tools** (`bearoffid2pos`, `bearoffprobs`)
* **Legal-move enumeration** (`moves`) & **probabilistic evaluation** (`probs`)
* **Monte-Carlo rollouts** (`rollout`, `crollout`)
* **Equity lookup** (`equities.value(xAway, oAway)`)
* **Runtime engine tuning** via the `set` submodule

## Testing

GNUBG-nn-pypi requires [pytest](https://pytest.org/). After installation, run:

```bash
python -c "import gnubg, sys; sys.exit(gnubg.test() is False)"
```

## Code of Conduct

This project is driven by a diverse community. Please read our [Code of Conduct](https://github.com/gnubg/gnubg-nn-pypi/blob/main/CODE_OF_CONDUCT.md) to learn how to interact positively and inclusively.

## Contributing

Your expertise and enthusiasm are welcome! You can contribute by:

* Reviewing and testing pull requests
* Reporting and triaging issues
* Improving documentation, tutorials, and examples
* Enhancing engine parameters or submodules
* Maintaining website or branding assets
* Translating materials
* Assisting with outreach and onboarding
* Writing grant proposals or helping with fundraising

For more information, see our [Contributing Guide](https://github.com/gnubg/gnubg-nn-pypi/blob/main/CONTRIBUTING.md). If you’re unsure where to start, open an issue or join the discussion on our mailing list!
