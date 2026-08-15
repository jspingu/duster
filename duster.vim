syn clear
syn iskeyword @,48-57,192-255,$,_,.,+,-,*,/,%,<,>,:,!,&,@-@,=

syn match Number /\-\?[0-9]*\.\?[0-9]\+/
syn match Number /\-\?[0-9]\+\.\?[0-9]*/
syn match Ignore /[^\. \t(){}\[\]]*\.[^\. \t(){}\[\]]*\.[^ \t(){}\[\]]*/
syn match Ignore /[^ \t(){}\[\]]\+\-[^ \t(){}\[\]]*/
syn match Ignore /[^ \t(){}\[\]]*[^0-9\.\- \t(){}\[\]][^ \t(){}\[\]]*/
syn match Delimiter /[(){}\[\]]/

"Control
syn keyword Keyword inline endinline if elif else then endif while do endwhile jmp jz call unnest

"Self-modification
syn keyword Keyword dsp ->ds <-ds !ds @ds stub resolve capture push : :&

"Arithmetic
syn keyword Keyword + - * / % f+ f- f* f/ ++ --

"Logic / Comparison
syn keyword Keyword and or not < > = <= >=

"Stack
syn keyword Identifier swap dup drop rise sink over 2drop 2dup ndrop ndup nrise nsink nover

"Constants
syn keyword Constant pi 2pi

syn region String start=/"/ end=/"/
syn region Comment start=/\/\// end=/$/
