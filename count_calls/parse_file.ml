(* open Lexer *)
open Lexing

let print_position outx lexbuf =
  let pos = lexbuf.lex_curr_p in
  Printf.fprintf outx "%s:%d:%d" pos.pos_fname
    pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1)

let wrap_error parse_fn lexbuf: 'a option =
  try Some (parse_fn lexbuf) with
  | Lexer.SyntaxError msg ->
    Printf.eprintf "%a: %s\n" print_position lexbuf msg;
    None
  | Parser.Error ->
    Printf.eprintf "%a: syntax error\n" print_position lexbuf;
    None (* exit (-1) *);;

let parse_file lex_and_parse filename =
  let inx = open_in filename in
  let lexbuf = Lexing.from_channel inx in
  lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_fname = filename };
  let out = wrap_error lex_and_parse lexbuf in
  close_in inx; out

