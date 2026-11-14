VERSION := 0.2.0
.PHONY: all test package clean
all:
	$(MAKE) -C src
test: all
	@set -e; for app in src/objects.*-release/Lister; do "$$app" --self-test; done
package: all
	sh packaging/package.sh $(VERSION)
clean:
	$(MAKE) -C src clean
