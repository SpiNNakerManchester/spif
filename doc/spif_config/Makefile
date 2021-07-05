# Copyright (c) 2017-2019 The University of Manchester
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

spif_config.pdf: spif_config.tex spif_bd.fig spif_cf.fig spin_per_if.fig
	fig2mpdf spif_bd.fig
	fig2mpdf spif_cf.fig
	fig2mpdf spin_per_if.fig
	pdflatex spif_config.tex
	pdflatex spif_config.tex

clean:
	/bin/rm -f *.dvi *.pdf *.log *.aux
