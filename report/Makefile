BUILD_DIR := build
OUTDIR := $(BUILD_DIR)/output
LATEXMKRC := $(BUILD_DIR)/latexmkrc
SRC := main.tex
PDF := $(basename $(SRC)).pdf
LATEXMK := latexmk

.PHONY: all clean

all:
	mkdir -p "$(OUTDIR)"
	$(LATEXMK) -r "$(LATEXMKRC)" -cd -pdf -interaction=nonstopmode -halt-on-error -outdir="$(OUTDIR)" "$(SRC)"
	cp "$(OUTDIR)/$(PDF)" "$(PDF)"

clean:
	$(LATEXMK) -r "$(LATEXMKRC)" -cd -C -outdir="$(OUTDIR)" "$(SRC)"
	rm -rf "$(OUTDIR)"
	rm -f "$(PDF)"
