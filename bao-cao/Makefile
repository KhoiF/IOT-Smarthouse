# Biên dịch báo cáo: make (3 lượt để số trang trong mục lục/danh mục ổn định)  (cần XeLaTeX và phông Times New Roman)
main.pdf: main.tex $(wildcard hinh/*)
	xelatex -interaction=nonstopmode main.tex
	xelatex -interaction=nonstopmode main.tex
	xelatex -interaction=nonstopmode main.tex

clean:
	rm -f *.aux *.log *.out *.toc *.lof *.lot build.log

.PHONY: clean
