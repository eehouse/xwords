
tags:
	etags $$(find . -name '*.c' -print \
		-o -name '*.h' -print \
		-o -name '*.cpp' -print \
	)
	FILES=""; \
	for f in $$(find android/ -name '*.java' -o -name '*.xml'); do \
		DIR=$$(dirname $$f); \
		if [ -e $$DIR/.gitignore ]; then \
			if grep -q $$(basename $$f) $$DIR/.gitignore; then \
				echo "skipping $$f"; \
				f=""; \
			fi; \
		fi; \
		[ "$$f" != "" ] && FILES="$$FILES $$f"; \
	done; \
	etags -a $$FILES
