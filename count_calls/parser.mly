%{
open Calls

let minus_one s =
   String.sub s 0 ((String.length s) - 1);;
%}

%token ENTER
%token LEAVE
%token NEWLINE
(* %token COLON *)
%token <string> ARG
%token <string> ID
%token EOF

%start < Calls.symb list > calls
%%

calls: ccc = rev_calls EOF { List.rev ccc };

rev_calls:
| ccc = rev_calls; ENTER; id = ID;  a = args; NEWLINE
  { Enter(minus_one id, a)::ccc }
| ccc = rev_calls; LEAVE; id = ID; ARG; a = args; NEWLINE
  { Leave(minus_one id, a)::ccc }
| { [] }
;

(* args: aaa = rev_args NEWLINE EOF { List.rev aaa}; *)
args: aaa = rev_args { List.rev aaa};

rev_args:
| (* empty *) { [] }
| aaa = rev_args; a = ID { a::aaa }
| aaa = rev_args; a = ARG { a::aaa }
;
