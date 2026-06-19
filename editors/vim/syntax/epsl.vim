if exists("b:current_syntax")
  finish
endif

syn match epslComment "//.*$"
syn region epslComment start="/\*" end="\*/" keepend

syn region epslString start=+"+ skip=+\\\\\|\\"+ end=+"+
syn region epslString start=+'+ skip=+\\\\\\|\\'+ end=+'+

syn keyword epslKeyword import if else while for return break continue none fn in and or do
syn keyword epslBoolean true false
syn match epslNumber /\v\<\d+(\.\d+)?([eE][+-]?\d+)?\>/
syn match epslFunc /\v\w+\ze\s*\(/
syn match epslBracket /\v[\[\]]/
syn match epslOperator /[-+*\/=<>!&|]+/

hi def link epslMacro PreProc
hi def link epslComment Comment
hi def link epslString String
hi def link epslKeyword Keyword
hi def link epslBoolean Boolean
hi def link epslNumber Number
hi def link epslFunc Function
hi def link epslOperator Operator
hi def link epslBracket Special

let b:current_syntax = "epsl"
