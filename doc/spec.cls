\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{spec}

\ExecuteOptions{11pt,a4paper}
\ProcessOptions

\LoadClass{article}

% UTF-8 input
\RequirePackage{elatex}
\RequirePackage{ucs}
\RequirePackage[utf8x]{inputenc}
\RequirePackage[english]{babel}
\RequirePackage[margin=3cm]{geometry}
\RequirePackage{amsmath}
\RequirePackage{amssymb}
\RequirePackage{latexsym}
\RequirePackage{amsxtra}
\RequirePackage{amscd}
\RequirePackage{charter}
\RequirePackage{fancyhdr}
\RequirePackage{graphicx}
\RequirePackage{color}
\RequirePackage{hyperref}
\RequirePackage{url}
\pagestyle{plain}

% dont indent paragraphs
\setlength{\parindent}{0pt}
\setlength{\parskip}{8pt}

% some highlighting
% \todo (no args)
\definecolor{red}{rgb}{1,0,0}
\newcommand{\todo}{\textbf\textsf{\color{red}TODO: }}
% \ann{text...}
\definecolor{brown}{rgb}{0.66,0.33,0}
\newcommand{\ann}[1]{\textit\textsf{\color{brown}#1}}

% RFC2119, use like "x \should\ blah"
% (dont forget the \ at the end or it will stick to the next word)
\newcommand{\may}{\textsc{may}}
\newcommand{\should}{\textsc{should}}
\newcommand{\shouldnot}{\textsc{should not}}
\newcommand{\must}{\textsc{must}}
\newcommand{\mustnot}{\textsc{must not}}

% oc, as in override code - use: \oc{fade} (no \ inside!)
\newcommand{\oc}[1]{\texttt{\textbackslash #1}}

\makeatletter

\def\subtitle#1{\gdef\@subtitle{#1}}
\subtitle{\relax}
\def\contrib#1{\gdef\@contrib{#1}}
\contrib{\relax}

\newcommand{\spectitle}{
\begin{titlepage}
\setlength{\parskip}{0pt}
{\Huge \@title}\par
\vspace{4pt}
\hrule\par
%
\vspace{8pt}
\if\@subtitle\relax \relax
\else {\large \@subtitle}
\fi
\hfill\today
%
\vspace{0.6cm}\par
\@author
\if\@contrib\relax \relax
\else \\\@contrib
\fi
%
\vspace{1cm}\par
\tableofcontents
\end{titlepage}
}

\makeatother

\endinput
