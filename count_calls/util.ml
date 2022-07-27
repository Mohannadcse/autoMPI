open Calls
open Lexer

let read_calls filename : symb list =
  match Parse_file.parse_file (Parser.calls Lexer.read) filename with
  | Some v -> v
  | None -> [];;

(* Finds all the call-graph paths from the top parameter to calls with name
   'name' *)
let find_sub_call top name : string list list =
  let finds : (string list list ref) = ref [] in
  let rec aux ctxt call =
    let Call (n, subs) = call in
    let ctxt_new = n::ctxt in
    if n = name then
      finds := ctxt_new :: (!finds)
    else
      List.iter (aux ctxt_new) subs
  in
  aux [] top;
  List.rev_map List.rev !finds;;

(* This is has type symb list -> (accum argument) -> (accum argument) -> call.
   Call with empty lists for the second and third arguments. *)
(* names is the stack of names of callers. context is their children. *)
let rec nested seq names context =
  let finish (e::names') (childs::context') rest check = begin
    check e;
    let call = (let n, _ = e in Call(n, childs)) in
    begin match context' with
    | parent :: context'' -> nested rest names' ((call::parent) :: context'')
    | [] -> call
    end
  end in
  match seq with
  | (Enter e)::seq_r -> nested seq_r (e::names) ([] :: context)
  | (Leave l)::seq_r -> let err e =
                          if e <> l then
                            let (e_n, e_l) = e in
                            let (l_n, l_l) = l in
                            let rem_n = List.length seq_r in
                            let print2 a b =
                              Printf.printf "%s %s\n" a b
                            in
                            Printf.printf "Remaining lines: %d\n" rem_n;
                            Printf.printf "Names: %s %s\n" e_n l_n;
                            (let e_l_l = List.length e_l in
                             let l_l_l = List.length l_l in
                             Printf.printf "# of Args: %d %d\n" e_l_l l_l_l);
                            (try List.iter2 print2 e_l l_l with
                              Invalid_argument _ -> ());
                            raise (Invalid_argument "unmatched")
                        in
                        finish names context seq_r err

  | []               -> finish names context []    (fun e -> ());;

