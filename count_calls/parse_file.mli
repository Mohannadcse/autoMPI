(* 't will be the token type from the parser
and 'a will be the type of the start token *)
val parse_file : (Lexing.lexbuf -> 'a) -> string -> 'a option;;
