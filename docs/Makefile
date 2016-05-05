# Paperwork and statement in pdf
DOC_SOURCES := $(wildcard *.md)
DOC_TARGETS := $(DOC_SOURCES:.md=.pdf)

docs: $(DOC_TARGETS)
	@echo > /dev/null

%.pdf: %.md
	$(info [doc] $< -> $@)
	@pandoc $(PANDOC_FLAGS) --from=markdown        \
												  --latex-engine=xelatex \
													--to=latex $<          \
													-o $@

