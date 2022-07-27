{
open Lexing
open Parser

exception SyntaxError of string

let next_line lexbuf =
  let pos = lexbuf.lex_curr_p in
  lexbuf.lex_curr_p <-
    { pos with pos_bol = lexbuf.lex_curr_pos;
               pos_lnum = pos.pos_lnum + 1
    }
}

let chr = "%$" _
let id_chr = [ '_' 'a'-'z' 'A'-'Z' '.' '$' '-']
let id = id_chr  (id_chr | ['_' 'a'-'z' 'A'-'Z' '.' '$' '-' '0'-'9' ])* ':'
let newline = '\r' | '\n' | "\r\n"
let white = [' ' '\t']+
let not_white = [^ ' ' '\t' '\r' '\n' ]+

rule read =
  parse
  | chr { ARG(Lexing.lexeme lexbuf) }
  | white { read lexbuf }
  | "leave" { LEAVE }
  | "enter" { ENTER }
  (* | ':' { COLON } *)
  | newline {  next_line lexbuf; NEWLINE }
  | id { ID(Lexing.lexeme lexbuf) }
  | not_white { ARG(Lexing.lexeme lexbuf) }
  | _ { raise (SyntaxError ("Unexpected char: " ^ Lexing.lexeme lexbuf)) }
  | eof { EOF }

