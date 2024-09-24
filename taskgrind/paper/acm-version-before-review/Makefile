all:
	pdflatex article
	- bibtex article
	pdflatex article
	- makeindex -s gind.ist -o article.ind article.idx
	- makeindex -s gglo.ist -o article.gls article.glo
	pdflatex article
	while ( grep -q '^LaTeX Warning: Label(s) may have changed' article.log) \
		do pdflatex article; done

clean:
	rm -f  *.log *.aux \
		*.cfg *.glo *.idx *.toc \
		*.ilg *.ind *.out *.lof \
	   	*.lot *.bbl *.blg *.gls *.cut *.hd \
		*.dvi *.ps *.thm *.tgz *.zip *.rpi

fclean: clean
	rm -f article.pdf
